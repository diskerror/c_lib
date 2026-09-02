// EmbeddingCodec.cp — see EmbeddingCodec.h for the on-disk format contract.

#include "EmbeddingCodec.h"
#include "VectorMath.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace Diskerror::EmbeddingCodec {

// -----------------------------------------------------------------------
// Scalar dtype conversions. In-memory values are always f32; these convert
// to/from the packed on-disk representations.
// -----------------------------------------------------------------------

// IEEE 754 half-precision (f16) conversions.
// When _Float16 is available (Apple Silicon, GCC 12+/Clang 15+ on x86-64),
// use the compiler's native type for correct round-to-nearest conversions.
// Otherwise, fall back to manual bit manipulation.

#ifdef __FLT16_MAX__  // compiler supports _Float16

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

#else  // manual bit manipulation fallback

static inline uint16_t f32_to_f16(float f) {
    uint32_t x;
    std::memcpy(&x, &f, sizeof(x));
    const uint32_t sign = (x >> 16) & 0x8000u;
    // Strip sign, work with magnitude.
    x &= 0x7fffffffu;
    // NaN → keep a NaN (preserve sign, set a mantissa bit).
    if (x > 0x7f800000u) return static_cast<uint16_t>(sign | 0x7e00u);
    // Inf or too large → f16 infinity.
    if (x >= 0x47800000u) return static_cast<uint16_t>(sign | 0x7c00u);
    // Too small → flush to signed zero (no subnormal output for simplicity).
    if (x < 0x33000000u) return static_cast<uint16_t>(sign);
    // Subnormal f16 range.
    if (x < 0x38800000u) {
        // Re-bias into the subnormal range and round.
        uint32_t mantissa = (x & 0x007fffffu) | 0x00800000u;
        int shift = 113 - static_cast<int>(x >> 23);
        mantissa >>= shift;
        // Round to nearest even.
        mantissa += 1;
        mantissa >>= 1;
        return static_cast<uint16_t>(sign | mantissa);
    }
    // Normal range: re-bias exponent (127→15), round mantissa to 10 bits.
    // Round-to-nearest-even: add rounding bias based on truncated bits.
    uint32_t rebias = x + 0xc8000000u;  // subtract (127-15) << 23 via unsigned wrap
    uint32_t lsb = (rebias >> 13) & 1u;
    rebias += 0x00000fffu + lsb;         // round to nearest even
    return static_cast<uint16_t>(sign | (rebias >> 13));
}

static inline float f16_to_f32(uint16_t bits) {
    const uint32_t sign = (static_cast<uint32_t>(bits) & 0x8000u) << 16;
    uint32_t exponent   = (bits >> 10) & 0x1fu;
    uint32_t mantissa   = bits & 0x03ffu;
    uint32_t result;
    if (exponent == 0) {
        if (mantissa == 0) {
            result = sign;  // ±0
        } else {
            // Subnormal: renormalize.
            exponent = 1;
            while ((mantissa & 0x0400u) == 0) {
                mantissa <<= 1;
                exponent -= 1;
            }
            mantissa &= 0x03ffu;
            result = sign | ((exponent + 112u) << 23) | (mantissa << 13);
        }
    } else if (exponent == 31) {
        // Inf or NaN: re-bias exponent to f32.
        result = sign | 0x7f800000u | (mantissa << 13);
    } else {
        // Normal: re-bias exponent (15→127).
        result = sign | ((exponent + 112u) << 23) | (mantissa << 13);
    }
    float f;
    std::memcpy(&f, &result, sizeof(f));
    return f;
}

#endif  // __FLT16_MAX__

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
// Blob read/write helpers. Target hosts are little-endian (ARM64, x86-64),
// so the in-memory byte order matches the on-disk byte order and these are
// plain memcpy. Using typed helpers keeps call sites readable.
// -----------------------------------------------------------------------
static inline void put_u16(uint8_t* p, uint16_t v) { std::memcpy(p, &v, 2); }
static inline uint16_t get_u16(const uint8_t* p) {
    uint16_t v; std::memcpy(&v, p, 2); return v;
}
static inline void put_f32(uint8_t* p, float f) { std::memcpy(p, &f, 4); }
static inline float get_f32(const uint8_t* p) {
    float f; std::memcpy(&f, p, 4); return f;
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

int payload_size(VectorType t, int dims) {
    int suffix = (t == VectorType::INT8) ? 2 : 0;  // f16 scale at end for int8
    return dims * payload_stride(t) + suffix;
}

int expected_blob_size(VectorType t, int dims, int offset) {
    return offset + payload_size(t, dims);
}

std::vector<uint8_t> encode(VectorType t, const std::vector<float>& v,
                            int offset, uint8_t version) {
    const int dims = static_cast<int>(v.size());

    std::vector<uint8_t> out(offset + payload_size(t, dims));
    if (offset > 0) {
        std::memset(out.data(), 0, offset);
        out[0] = version;
    }
    uint8_t* payload = out.data() + offset;

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
        put_u16(payload + dims, f32_to_f16(scale));
    } else {
        switch (t) {
            case VectorType::F32:
                for (int i = 0; i < dims; ++i) put_f32(payload + i * 4, v[i]);
                break;
            case VectorType::F16:
                for (int i = 0; i < dims; ++i)
                    put_u16(payload + i * 2, f32_to_f16(v[i]));
                break;
            case VectorType::BF16:
                for (int i = 0; i < dims; ++i)
                    put_u16(payload + i * 2, f32_to_bf16(v[i]));
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
            VectorType t, std::vector<float>& out, int offset) {
    out.assign(static_cast<size_t>(expected_dims), 0.0f);
    if (blob == nullptr || expected_dims <= 0 || blob_bytes <= offset) return false;
    const uint8_t* payload = static_cast<const uint8_t*>(blob) + offset;

    const int expect_size = payload_size(t, expected_dims);
    if (blob_bytes - offset != expect_size) return false;

    if (t == VectorType::INT8) {
        float scale = f16_to_f32(get_u16(payload + expected_dims));
        for (int i = 0; i < expected_dims; ++i)
            out[i] = static_cast<float>(static_cast<int8_t>(payload[i])) * scale;
    } else {
        switch (t) {
            case VectorType::F32:
                for (int i = 0; i < expected_dims; ++i)
                    out[i] = get_f32(payload + i * 4);
                break;
            case VectorType::F16:
                for (int i = 0; i < expected_dims; ++i)
                    out[i] = f16_to_f32(get_u16(payload + i * 2));
                break;
            case VectorType::BF16:
                for (int i = 0; i < expected_dims; ++i)
                    out[i] = bf16_to_f32(get_u16(payload + i * 2));
                break;
            case VectorType::INT8:
                break;  // unreachable
        }
    }
    return true;
}

bool decode_infer_dims(const void* blob, int blob_bytes, VectorType t,
                       int offset, std::vector<float>& out, int min_dims) {
    out.clear();
    if (blob == nullptr || blob_bytes <= offset || offset < 0) return false;
    const uint8_t* payload = static_cast<const uint8_t*>(blob) + offset;
    const int payload_bytes = blob_bytes - offset;
    const int stride = payload_stride(t);

    if (t == VectorType::INT8) {
        const int dims_with_scale = payload_bytes - 2;
        const int dims_without    = payload_bytes;
        int dims;
        float scale;
        if (dims_with_scale >= min_dims) {
            dims = dims_with_scale;
            scale = f16_to_f32(get_u16(payload + dims));
            if (!(scale > 0.0f) || !std::isfinite(scale)) scale = 1.0f / 127.0f;
        } else if (dims_without >= min_dims) {
            dims = dims_without;
            scale = 1.0f / 127.0f;  // unit-norm fallback
        } else {
            return false;
        }
        out.assign(static_cast<size_t>(dims), 0.0f);
        for (int i = 0; i < dims; ++i)
            out[i] = static_cast<float>(static_cast<int8_t>(payload[i])) * scale;
        return true;
    }

    if (stride <= 0 || payload_bytes % stride != 0) return false;
    const int dims = payload_bytes / stride;
    if (dims < min_dims) return false;

    out.assign(static_cast<size_t>(dims), 0.0f);
    switch (t) {
        case VectorType::F32:
            for (int i = 0; i < dims; ++i) out[i] = get_f32(payload + i * 4);
            break;
        case VectorType::F16:
            for (int i = 0; i < dims; ++i) out[i] = f16_to_f32(get_u16(payload + i * 2));
            break;
        case VectorType::BF16:
            for (int i = 0; i < dims; ++i) out[i] = bf16_to_f32(get_u16(payload + i * 2));
            break;
        case VectorType::INT8:
            break;  // unreachable
    }
    return true;
}

}  // namespace Diskerror::EmbeddingCodec
