// Unit tests for ragger::vector_codec — round-trip fidelity across storage
// dtypes, version-tagged format, and legacy blob decoding. main()-based per
// this project's test convention.

#include "vector_codec.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <random>
#include <string>
#include <vector>

using namespace Diskerror::vector_codec;

static int failures = 0;

static void check(bool cond, const char* what) {
    if (!cond) {
        std::printf("FAIL %s\n", what);
        ++failures;
    }
}

// Mean absolute error of a round-trip through dtype `t` at version `ver`.
static float roundtrip_mae(VectorType t, const std::vector<float>& v,
                           uint8_t ver = 42) {
    std::vector<uint8_t> blob = encode(t, v, ver);
    std::vector<float> back;
    bool ok = decode(blob.data(), static_cast<int>(blob.size()),
                     static_cast<int>(v.size()), t, back);
    if (!ok || back.size() != v.size()) return 1e9f;
    float acc = 0.0f;
    for (size_t i = 0; i < v.size(); ++i) acc += std::fabs(v[i] - back[i]);
    return acc / static_cast<float>(v.size());
}

int main() {
    // --- string parsing / canonicalization -----------------------------
    check(parse("f32").value() == VectorType::F32, "parse f32");
    check(parse("F16").value() == VectorType::F16, "parse F16 (case-insensitive)");
    check(parse("bf16").value() == VectorType::BF16, "parse bf16");
    check(parse("int8").value() == VectorType::INT8, "parse int8");
    check(parse("bfloat16").value() == VectorType::BF16, "parse bfloat16 alias");
    check(!parse("f8").has_value(), "parse unknown -> nullopt");
    check(canonical("garbage") == "f16", "canonical fallback -> f16");
    check(canonical("BF16") == "bf16", "canonical normalizes case");
    check(is_supported("int8") && !is_supported("q4"), "is_supported");
    check(std::string(supported_csv()) == "f32,f16,bf16,int8", "supported_csv");

    // --- deterministic normalized-ish embedding ------------------------
    std::mt19937 rng(12345);
    std::normal_distribution<float> nd(0.0f, 0.3f);
    std::vector<float> v(384);
    for (auto& x : v) x = nd(rng);

    // f32 must be exactly lossless.
    check(roundtrip_mae(VectorType::F32, v) == 0.0f, "f32 lossless round-trip");

    // f16/bf16/int8 lossy but bounded. Thresholds are generous but catch
    // gross breakage (wrong shift, endianness, missing dequant scale).
    float mae_f16  = roundtrip_mae(VectorType::F16, v);
    float mae_bf16 = roundtrip_mae(VectorType::BF16, v);
    float mae_int8 = roundtrip_mae(VectorType::INT8, v);
    std::printf("MAE  f16=%.6g  bf16=%.6g  int8=%.6g\n", mae_f16, mae_bf16, mae_int8);
    check(mae_f16  < 1e-3f, "f16 round-trip within tolerance");
    check(mae_bf16 < 5e-3f, "bf16 round-trip within tolerance");
    check(mae_int8 < 5e-3f, "int8 round-trip within tolerance");
    // f16 has more mantissa bits than bf16 -> should be more accurate here.
    check(mae_f16 < mae_bf16, "f16 more accurate than bf16 for small values");

    // --- blob sizes (version byte + payload, int8 gets +2 for f16 scale) --
    check(encode(VectorType::F32,  v, 0).size() == 1u + 384u * 4, "f32 blob size");
    check(encode(VectorType::F16,  v, 0).size() == 1u + 384u * 2, "f16 blob size");
    check(encode(VectorType::BF16, v, 0).size() == 1u + 384u * 2, "bf16 blob size");
    check(encode(VectorType::INT8, v, 0).size() == 1u + 384u * 1 + 2u, "int8 blob size");

    // --- expected_blob_size matches actual encode output ----------------
    for (auto t : {VectorType::F32, VectorType::F16, VectorType::BF16, VectorType::INT8}) {
        check(static_cast<int>(encode(t, v, 7).size()) == expected_blob_size(t, 384),
              "expected_blob_size matches encode");
    }

    // --- version byte is correctly stored and read ----------------------
    {
        auto blob = encode(VectorType::F16, v, 99);
        check(blob_version(blob.data(), (int)blob.size()) == 99, "version byte stored");
        // Decode with correct dtype should succeed regardless of version value.
        std::vector<float> back;
        bool ok = decode(blob.data(), (int)blob.size(), 384, VectorType::F16, back);
        check(ok, "decode succeeds for version-tagged blob");
    }

    // --- version byte 0 works (edge case) ------------------------------
    {
        auto blob = encode(VectorType::F16, v, 0);
        check(blob_version(blob.data(), (int)blob.size()) == 0, "version 0 works");
    }

    // --- f16 vs bf16 are distinguishable via the caller knowing dtype ---
    // (caller gets dtype from settings, not from the blob itself)
    {
        std::vector<float> one = {2.5f};
        auto bf = encode(VectorType::BF16, one, 1);
        std::vector<float> back;
        check(decode(bf.data(), (int)bf.size(), 1, VectorType::BF16, back) &&
              std::fabs(back[0] - 2.5f) < 1e-6f,
              "bf16 decodes correctly with dtype hint");
        // Decoding bf16 blob as f16 should fail (size matches but produces wrong answer
        // only if dims match — with 1 dim both are 3 bytes so it succeeds; that's fine,
        // the point is the CALLER passes the correct dtype from settings).
    }

    // --- dimension-mismatch rejection ----------------------------------
    {
        auto blob = encode(VectorType::F16, v, 5);
        std::vector<float> back;
        bool ok = decode(blob.data(), (int)blob.size(), 128 /*wrong*/, VectorType::F16, back);
        check(!ok, "dim mismatch rejected");
        check(back.size() == 128 && back[0] == 0.0f, "dim mismatch -> zero vector");
    }

    // --- null / empty ---------------------------------------------------
    {
        std::vector<float> back;
        check(!decode(nullptr, 0, 384, VectorType::F16, back), "null blob rejected");
        check(back.size() == 384, "null -> zero vector of expected dims");
    }

    // --- blob_version edge cases ----------------------------------------
    check(blob_version(nullptr, 0) == -1, "blob_version null -> -1");
    {
        uint8_t b = 255;
        check(blob_version(&b, 1) == 255, "blob_version 255");
    }

    // --- int8 f16 scale precision test ----------------------------------
    // Verify the f16 scale is sufficient for unit-norm embeddings.
    {
        // Normalize v to unit norm like real embeddings
        float norm = 0.0f;
        for (float x : v) norm += x * x;
        norm = std::sqrt(norm);
        std::vector<float> vn(v.size());
        for (size_t i = 0; i < v.size(); ++i) vn[i] = v[i] / norm;

        float mae = roundtrip_mae(VectorType::INT8, vn);
        std::printf("MAE  int8 unit-norm=%.6g\n", mae);
        check(mae < 5e-3f, "int8 unit-norm round-trip within tolerance");
    }

    // --- LEGACY decode_any: RV1 headered blobs -------------------------
    // Build an old-format RV1 blob by hand and verify decode_any reads it.
    {
        const int dims = 8;
        std::vector<float> src(dims);
        for (int i = 0; i < dims; ++i) src[i] = 0.1f * static_cast<float>(i - 4);

        // Build RV1 f16 blob manually
        std::vector<uint8_t> rv1(12 + dims * 2);
        rv1[0] = 'R'; rv1[1] = 'V'; rv1[2] = '1';
        rv1[3] = static_cast<uint8_t>(VectorType::F16);
        rv1[4] = 0; rv1[5] = 0;
        // dims as u16le at offset 6
        rv1[6] = static_cast<uint8_t>(dims & 0xff);
        rv1[7] = static_cast<uint8_t>((dims >> 8) & 0xff);
        // scale as f32le at offset 8 (0 for float types)
        rv1[8] = rv1[9] = rv1[10] = rv1[11] = 0;
        // payload: f16 values
        for (int i = 0; i < dims; ++i) {
            _Float16 h = static_cast<_Float16>(src[i]);
            uint16_t bits;
            std::memcpy(&bits, &h, sizeof(bits));
            rv1[12 + i*2]   = static_cast<uint8_t>(bits & 0xff);
            rv1[12 + i*2+1] = static_cast<uint8_t>((bits >> 8) & 0xff);
        }

        std::vector<float> back;
        bool ok = decode_any(rv1.data(), (int)rv1.size(), dims, VectorType::F16, back);
        check(ok, "decode_any: legacy RV1 f16 decodes");
        float mae = 0;
        for (int i = 0; i < dims; ++i) mae += std::fabs(src[i] - back[i]);
        check(mae / dims < 1e-3f, "decode_any: legacy RV1 f16 values correct");
    }

    // --- LEGACY decode_any: raw f16 (headerless) -----------------------
    {
        const int dims = 8;
        std::vector<float> src(dims);
        for (int i = 0; i < dims; ++i) src[i] = 0.1f * static_cast<float>(i - 4);
        std::vector<uint16_t> raw(dims);
        for (int i = 0; i < dims; ++i) {
            _Float16 h = static_cast<_Float16>(src[i]);
            std::memcpy(&raw[i], &h, sizeof(uint16_t));
        }
        std::vector<float> back;
        bool ok = decode_any(raw.data(), dims * 2, dims, VectorType::F16, back);
        check(ok, "decode_any: legacy raw-f16 decodes");
        float mae = 0; for (int i=0;i<dims;++i) mae += std::fabs(src[i]-back[i]);
        check(mae / dims < 1e-3f, "decode_any: legacy raw-f16 values correct");
    }
    // Raw f32 (headerless)
    {
        const int dims = 8;
        std::vector<float> src(dims);
        for (int i = 0; i < dims; ++i) src[i] = 0.1f * static_cast<float>(i - 4);
        std::vector<float> back;
        bool ok = decode_any(src.data(), dims * 4, dims, VectorType::F16, back);
        check(ok, "decode_any: legacy raw-f32 decodes");
        float mae = 0; for (int i=0;i<dims;++i) mae += std::fabs(src[i]-back[i]);
        check(mae == 0.0f, "decode_any: legacy raw-f32 lossless");
    }

    if (failures == 0) std::printf("all vector_codec tests passed\n");
    else               std::printf("%d vector_codec test(s) FAILED\n", failures);
    return failures == 0 ? 0 : 1;
}
