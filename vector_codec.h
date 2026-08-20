// vector_codec.h — on-disk embedding (de)serialization across storage dtypes.
//
// Embeddings are computed and compared in float32. On disk we can trade
// precision for space: a 384-dim vector is 1536 B as f32, 768 B as f16/bf16,
// 384 B as int8. All in-memory math stays f32; this layer only governs the
// stored BLOB.
//
// VERSION-TAGGED FORMAT (v2, Aug 2026)
// ------------------------------------
// The first byte of every blob is a version tag (1–255) that must match the
// DB-wide `embedding_version` in the settings table. Version 0 is reserved
// as a sentinel for empty/placeholder blobs that were never properly
// embedded. Real versions cycle 1 → 255 → 1. The version number is
// incremented whenever the embedding configuration changes (model, dtype,
// dimensions). Any blob whose first byte doesn't match the current version
// is stale and must be re-embedded.
//
// The dtype, dimensions, and model identity are stored once in the settings
// table — NOT repeated per blob. This eliminates the old 12-byte RV1
// self-describing header.
//
//   ALL TYPES:
//     offset  size  field
//     0       1     version  (uint8, must match settings.embedding_version)
//
//   FLOAT TYPES (f32/f16/bf16):
//     1       ...   payload  (dims × stride bytes)
//
//   INT8:
//     1       ...   payload  (dims × 1 byte)
//     1+dims  2     scale    (float16, symmetric dequant multiplier)
//
// LEGACY BLOBS: the old format used a 12-byte header starting with 'R','V','1'.
// Even older blobs were raw f16/f32 with no header at all. Both are detected
// by decode() when the version byte doesn't match, and treated as stale —
// the caller should re-embed them. decode() can still read legacy formats
// for diagnostic purposes via decode_any().
//
// Endianness: all multi-byte values are little-endian. Float/half payloads
// are host-order memcpy (Ragger targets LE hosts: Apple Silicon, x86-64).

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Diskerror::vector_codec {

// Stable on-disk dtype codes — DO NOT renumber (referenced by settings table).
enum class VectorType : uint8_t {
    F32  = 0,  // IEEE 754 single precision (4 B/dim) — lossless baseline
    F16  = 1,  // IEEE 754 half (2 B/dim): 1s/5e/10m — good precision, small range
    BF16 = 2,  // bfloat16  (2 B/dim): 1s/8e/7m — f32's range, less mantissa
    INT8 = 3,  // symmetric per-vector int8 (1 B/dim + f16 scale) — 4x smaller than f32
};

// Parse a config string ("f32"/"f16"/"bf16"/"int8", case-insensitive) to a
// VectorType. std::nullopt if unrecognized.
std::optional<VectorType> parse(std::string_view s);

// Canonical lowercase name of a VectorType ("f32"/"f16"/"bf16"/"int8").
std::string to_string(VectorType t);

// Canonicalize a user/config string. Returns the canonical spelling when
// recognized; otherwise returns `fallback` (default "f16").
std::string canonical(std::string_view s, std::string_view fallback = "f16");

// True if `s` names a supported dtype.
bool is_supported(std::string_view s);

// Comma-separated list of supported dtypes, for config enum "allowed values"
// and validation ("f32,f16,bf16,int8").
std::string_view supported_csv();

// Bytes per dimension of the packed payload for a given dtype.
int payload_stride(VectorType t);

// Expected total blob size for a given dtype and dimension count (including
// the 1-byte version prefix and, for INT8, the 2-byte f16 scale).
int expected_blob_size(VectorType t, int dims);

// Encode a float32 vector into a version-tagged on-disk blob of the given dtype.
std::vector<uint8_t> encode(VectorType t, const std::vector<float>& v,
                            uint8_t version);

// Read the version byte from a blob. Returns -1 for null/empty blobs.
int blob_version(const void* blob, int blob_bytes);

// Decode a version-tagged blob of known dtype and dimensions into `out`
// (always resized to expected_dims). The version byte is NOT checked here —
// the caller is responsible for version gating. Returns true on success.
// On failure (null, size mismatch, etc.) `out` is filled with zeros and
// false is returned.
bool decode(const void* blob, int blob_bytes, int expected_dims,
            VectorType t, std::vector<float>& out);

// Decode any blob format — current version-tagged, old RV1-headered, or
// ancient raw f16/f32. For diagnostic/migration use only. Returns true on
// success. The `t` parameter is used as a hint for version-tagged blobs;
// for legacy formats the dtype is inferred from blob size.
bool decode_any(const void* blob, int blob_bytes, int expected_dims,
                VectorType t, std::vector<float>& out);

}  // namespace Diskerror::vector_codec
