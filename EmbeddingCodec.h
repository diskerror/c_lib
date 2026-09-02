// EmbeddingCodec.h — on-disk embedding (de)serialization across storage dtypes.
//
// Embeddings are computed and compared in float32. On disk we can trade
// precision for space: a 384-dim vector is 1536 B as f32, 768 B as f16/bf16,
// 384 B as int8. All in-memory math stays f32; this layer only governs the
// stored BLOB.
//
// OFFSET-PARAMETERIZED FORMAT
// ----------------------------
// Every blob is `offset` bytes of caller-defined header/prefix, followed by
// the packed payload. `offset` is a caller-supplied parameter, not implied
// by the format:
//
//   - Ragger (db_version 0.15+) stores payload-only blobs: offset = 0. The
//     embedding version tag lives in its own `embedding_version` column,
//     not in the blob.
//   - Pre-0.15 Ragger blobs (and other historic callers) prefixed a 1-byte
//     version tag before the payload: offset = 1.
//   - SemanticSQLite lets the user configure an arbitrary start offset per
//     DB when examining embeddings that don't start at byte 0 (e.g. blobs
//     with a foreign header this codec doesn't know about).
//
//   ALL TYPES:
//     [0, offset)          caller-defined header bytes (untouched by encode
//                          beyond an optional version tag — see encode()).
//     [offset, ...)        payload (dims × stride bytes)
//
//   INT8 only, appended after the float payload above:
//     [offset+dims, +2)    scale (float16, symmetric dequant multiplier)
//
// Endianness: little-endian hosts only (ARM64, x86-64). Blob byte order
// matches native memory layout; no byte-swapping is performed.

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Diskerror::EmbeddingCodec {

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

// Bytes of pure payload (dims × stride, plus the 2-byte f16 scale suffix for
// INT8) — does NOT include `offset`.
int payload_size(VectorType t, int dims);

// Expected total blob size for a given dtype, dimension count, and header
// offset: offset + payload_size(t, dims). `offset` defaults to 0 (Ragger's
// current payload-only format); pass 1 for the legacy single version-byte
// prefix, or a caller-configured value (e.g. SemanticSQLite).
int expected_blob_size(VectorType t, int dims, int offset = 0);

// Encode a float32 vector into a dtype-formatted blob preceded by `offset`
// header bytes. If offset >= 1, byte 0 is set to `version` (matching the
// historic single version-byte prefix convention); any remaining header
// bytes in [1, offset) are zero-filled. Pass offset = 0 for Ragger's
// payload-only blobs (version ignored in that case).
std::vector<uint8_t> encode(VectorType t, const std::vector<float>& v,
                            int offset = 0, uint8_t version = 0);

// Read the version byte (blob[0]) from a blob. Returns -1 for null/empty
// blobs. Only meaningful for blobs with offset >= 1.
int blob_version(const void* blob, int blob_bytes);

// Decode a blob of known dtype and dimensions into `out` (always resized to
// expected_dims), skipping the first `offset` header bytes before reading
// the payload. Validity requires blob_bytes > offset (there must be bytes
// beyond the header) AND blob_bytes - offset == payload_size(t, dims) (the
// payload region is exactly the right size). On failure (null, size
// mismatch, etc.) `out` is filled with zeros and false is returned.
bool decode(const void* blob, int blob_bytes, int expected_dims,
            VectorType t, std::vector<float>& out, int offset = 0);

}  // namespace Diskerror::EmbeddingCodec
