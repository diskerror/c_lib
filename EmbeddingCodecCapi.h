// EmbeddingCodecCapi.h — plain-C bridge to the C++ EmbeddingCodec
// implementation (EmbeddingCodec.h/.cp), so C translation units (e.g.
// SemanticSQLite's ext_functions.c/embed.c) can encode/decode embedding
// blobs without pulling in C++ headers.
//
// Decoding mirrors EmbeddingCodec's decode_infer_dims(): dims are inferred
// from the blob size when the caller has no settings table telling it the
// expected dimension count (SemanticSQLite's case — the user just points
// EMBEDDING_SIM/DIST at whatever blob column they have, with an optionally
// configured `offset`).
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Stable on-disk dtype codes — MUST match Diskerror::EmbeddingCodec::VectorType.
enum diskerror_vector_type {
    DISKERROR_VT_F32  = 0,
    DISKERROR_VT_F16  = 1,
    DISKERROR_VT_BF16 = 2,
    DISKERROR_VT_INT8 = 3,
};

// Decode `blob` (of the given dtype, `offset` header bytes skipped) into a
// malloc'd float array; dims are inferred from blob_bytes - offset:
//   - f32/f16/bf16: dims = payload_bytes / stride (payload_bytes must be
//     an exact multiple of stride).
//   - INT8: payload is `dims` bytes optionally followed by a 2-byte f16
//     scale suffix. Since a bare payload and a payload+scale can both look
//     "plausible", this prefers the with-scale interpretation whenever
//     payload_bytes - 2 >= min_dims, falling back to the without-scale
//     interpretation (implied scale 1/127, correct for unit-norm vectors)
//     otherwise.
// `min_dims` rejects implausibly small blobs (pass 16 to match real
// embedding models — this is what disambiguates the INT8 with/without-scale
// cases above without false positives in practice).
// On success, `*out_dims` receives the element count and the return value
// is a malloc'd array the caller must free() (or pass to
// diskerror_embedding_free()). Returns NULL on failure (bad blob, offset,
// or size) and sets `*out_dims` to 0.
float *diskerror_embedding_decode(int vtype, int offset,
                                  const void *data, int bytes,
                                  int min_dims, int *out_dims);

// Free an array returned by diskerror_embedding_decode().
void diskerror_embedding_free(float *p);

// Encode `dims` float32 values into a malloc'd blob of `offset` zeroed
// header bytes followed by the dtype-formatted payload (matching
// Diskerror::EmbeddingCodec::encode(t, v, offset, 0) — the header is
// zero-filled, not version-tagged, since SemanticSQLite has no version
// column). `*out_bytes` receives the total blob size (offset + payload).
// Returns NULL on allocation failure. Caller must free() the result (a
// plain byte buffer — diskerror_embedding_free() also works, it's just
// free()).
uint8_t *diskerror_embedding_encode(int vtype, int offset,
                                    const float *values, int dims,
                                    int *out_bytes);

#ifdef __cplusplus
}
#endif
