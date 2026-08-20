// vector_codec.cp — see vector_codec.h for the on-disk format contract.

#include "vector_codec.h"
#include "VectorMath.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace Diskerror::vector_codec {

// -----------------------------------------------------------------------
// Scalar dtype conversions. In-memory values are always f32; these convert
// to/from the packed on-disk representations.
// -----------------------------------------------------------------------

// IEEE 754 half. _Float16 is a native type on Apple Silicon (ARMv8.2-FP16)
// and modern x86 toolchains; the compiler emits correct round-to-nearest
// conversions, so we don't hand-roll the bit twiddling.
static inline uint16_t f32_to_f16(float f) {
    _Float16 h = static_cast<_Float16>(f);
    uint16_t bits;
    std::memcpy(&bits, &h, sizeof(bits));
    return bits;
}
static inline float f16_to_f32(uint16_t bits) {
    _Float16 h;
    std::memcpy(&h, &bits, sizeof(h));
    return static_cast<float>(h);
}

// bfloat16 = the high 16 bits of the float32 bit pattern. Same exponent range
// as f32 (8 exponent bits), only 7 mantissa bits. We round-to-nearest-even
// rather than truncate: add the rounding bias derived from the low 16 bits and
// the lsb of the retained half before shifting. NaN is preserved (never
// rounded into an infinity).
static inline uint16_t f32_to_bf16(float f) {
    uint32_t x;
    std::memcpy(&x, &f, sizeof(x));
    if ((x & 0x7fffffffu) > 0x7f800000u) {
        // NaN — keep it a NaN (set a mantissa bit in the top half).
        return static_cast<uint16_t>((x >> 16) | 0x0040u);
    }
    const uint32_t lsb          = (x >> 16) & 1u;
    const uint32_t rounding_bias = 0x7fffu + lsb;
    x += rounding_bias;
    return static_cast<uint16_t>(x >> 16);
}
static inline float bf16_to_f32(uint16_t bits) {
    uint32_t x = static_cast<uint32_t>(bits) << 16;
    float f;
    std::memcpy(&f, &x, sizeof(f));
    return f;
}

// -----------------------------------------------------------------------
// Little-endian (de)serialization helpers.
// -----------------------------------------------------------------------
static inline void put_u16le(uint8_t* p, uint16_t v) {
    p[0] = static_cast<uint8_t>(v & 0xff);
    p[1] = static_cast<uint8_t>((v >> 8) & 0xff);
}
static inline uint16_t get_u16le(const uint8_t* p) {
    return static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8);
}
static inline void put_f32le(uint8_t* p, float f) {
    uint32_t x;
    std::memcpy(&x, &f, sizeof(x));
    p[0] = static_cast<uint8_t>(x & 0xff);
    p[1] = static_cast<uint8_t>((x >> 8) & 0xff);
    p[2] = static_cast<uint8_t>((x >> 16) & 0xff);
    p[3] = static_cast<uint8_t>((x >> 24) & 0xff);
}
static inline float get_f32le(const uint8_t* p) {
    uint32_t x = static_cast<uint32_t>(p[0]) |
                 (static_cast<uint32_t>(p[1]) << 8) |
                 (static_cast<uint32_t>(p[2]) << 16) |
                 (static_cast<uint32_t>(p[3]) << 24);
    float f;
    std::memcpy(&f, &x, sizeof(f));
    return f;
}

// -----------------------------------------------------------------------
// Public API
// -----------------------------------------------------------------------

std::optional<VectorType> parse(std::string_view s) {
    // lowercase compare without allocating.
    auto eq = [&](std::string_view lit) {
        if (s.size() != lit.size()) return false;
        for (size_t i = 0; i < s.size(); ++i) {
            char c = s[i];
            if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
            if (c != lit[i]) return false;
        }
        return true;
    };
    if (eq("f32")  || eq("float32")) return VectorType::F32;
    if (eq("f16")  || eq("float16") || eq("half")) return VectorType::F16;
    if (eq("bf16") || eq("bfloat16")) return VectorType::BF16;
    if (eq("int8") || eq("i8") || eq("q8")) return VectorType::INT8;
    return std::nullopt;
}

std::string to_string(VectorType t) {
    switch (t) {
        case VectorType::F32:  return "f32";
        case VectorType::F16:  return "f16";
        case VectorType::BF16: return "bf16";
        case VectorType::INT8: return "int8";
    }
    return "f16";
}

std::string canonical(std::string_view s, std::string_view fallback) {
    if (auto t = parse(s)) return to_string(*t);
    return std::string(fallback);
}

bool is_supported(std::string_view s) { return parse(s).has_value(); }

std::string_view supported_csv() { return "f32,f16,bf16,int8"; }

int payload_stride(VectorType t) {
    switch (t) {
        case VectorType::F32:  return 4;
        case VectorType::F16:  return 2;
        case VectorType::BF16: return 2;
        case VectorType::INT8: return 1;
    }
    return 0;
}

int expected_blob_size(VectorType t, int dims) {
    int prefix = 1;  // version byte
    int suffix = (t == VectorType::INT8) ? 2 : 0;  // f16 scale at end for int8
    return prefix + dims * payload_stride(t) + suffix;
}

std::vector<uint8_t> encode(VectorType t, const std::vector<float>& v,
                            uint8_t version) {
    const int dims = static_cast<int>(v.size());

    std::vector<uint8_t> out(expected_blob_size(t, dims));
    uint8_t* p = out.data();

    // Version byte
    p[0] = version;

    // All types: payload starts at byte 1.
    uint8_t* payload = p + 1;

    if (t == VectorType::INT8) {
        // Symmetric per-vector int8 quantization: scale = max|x| / 127.
        VectorMath<float> vm(v);
        float maxabs = vm.max_mag();
        float scale = (maxabs > 0.0f) ? (maxabs / 127.0f) : 1.0f;

        const float inv = 1.0f / scale;
        for (int i = 0; i < dims; ++i) {
            float q = std::lround(v[i] * inv);
            q = std::clamp(q, -127.0f, 127.0f);
            payload[i] = static_cast<uint8_t>(static_cast<int8_t>(q));
        }

        // f16 scale as 2-byte suffix after the int8 payload.
        put_u16le(payload + dims, f32_to_f16(scale));
    } else {
        switch (t) {
            case VectorType::F32:
                for (int i = 0; i < dims; ++i) put_f32le(payload + i * 4, v[i]);
                break;
            case VectorType::F16:
                for (int i = 0; i < dims; ++i)
                    put_u16le(payload + i * 2, f32_to_f16(v[i]));
                break;
            case VectorType::BF16:
                for (int i = 0; i < dims; ++i)
                    put_u16le(payload + i * 2, f32_to_bf16(v[i]));
                break;
            case VectorType::INT8:
                break;  // handled above; unreachable
        }
    }
    return out;
}

int blob_version(const void* blob, int blob_bytes) {
    if (blob == nullptr || blob_bytes < 1) return -1;
    return static_cast<const uint8_t*>(blob)[0];
}

bool decode(const void* blob, int blob_bytes, int expected_dims,
            VectorType t, std::vector<float>& out) {
    out.assign(static_cast<size_t>(expected_dims), 0.0f);
    if (blob == nullptr || expected_dims <= 0 || blob_bytes < 1) return false;
    const uint8_t* p = static_cast<const uint8_t*>(blob);

    // Check size matches expected format (version byte already at p[0]).
    const int expect_size = expected_blob_size(t, expected_dims);
    if (blob_bytes != expect_size) return false;

    if (t == VectorType::INT8) {
        // payload at byte 1, f16 scale as 2-byte suffix after payload
        const uint8_t* payload = p + 1;
        float scale = f16_to_f32(get_u16le(payload + expected_dims));
        for (int i = 0; i < expected_dims; ++i)
            out[i] = static_cast<float>(static_cast<int8_t>(payload[i])) * scale;
    } else {
        const uint8_t* payload = p + 1;
        switch (t) {
            case VectorType::F32:
                for (int i = 0; i < expected_dims; ++i)
                    out[i] = get_f32le(payload + i * 4);
                break;
            case VectorType::F16:
                for (int i = 0; i < expected_dims; ++i)
                    out[i] = f16_to_f32(get_u16le(payload + i * 2));
                break;
            case VectorType::BF16:
                for (int i = 0; i < expected_dims; ++i)
                    out[i] = bf16_to_f32(get_u16le(payload + i * 2));
                break;
            case VectorType::INT8:
                break;  // unreachable
        }
    }
    return true;
}

}  // namespace Diskerror::vector_codec
