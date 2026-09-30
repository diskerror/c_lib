// EmbeddingCodecCapi.cp — C-linkage bridge over the C++ EmbeddingCodec API
// (Diskerror::EmbeddingCodec, std::vector), so plain-C callers can
// encode/decode embedding blobs without touching C++ headers directly.

#include "EmbeddingCodecCapi.h"
#include "EmbeddingCodec.h"

#include <cstdlib>
#include <cstring>
#include <vector>

extern "C" float *diskerror_embedding_decode(int vtype, int offset,
                                             const void *data, int bytes,
                                             int min_dims, int *out_dims) {
    if (out_dims) *out_dims = 0;
    auto t = static_cast<Diskerror::EmbeddingCodec::VectorType>(vtype);
    std::vector<float> v;
    if (!Diskerror::EmbeddingCodec::decode_infer_dims(data, bytes, t, offset, v, min_dims))
        return nullptr;
    float *out = static_cast<float *>(std::malloc(sizeof(float) * v.size()));
    if (!out) return nullptr;
    std::memcpy(out, v.data(), sizeof(float) * v.size());
    if (out_dims) *out_dims = static_cast<int>(v.size());
    return out;
}

extern "C" void diskerror_embedding_free(float *p) {
    std::free(p);
}

extern "C" uint8_t *diskerror_embedding_encode(int vtype, int offset,
                                               const float *values, int dims,
                                               int *out_bytes) {
    if (out_bytes) *out_bytes = 0;
    if (dims < 0) return nullptr;
    auto t = static_cast<Diskerror::EmbeddingCodec::VectorType>(vtype);
    std::vector<float> v(values, values + dims);
    std::vector<uint8_t> blob = Diskerror::EmbeddingCodec::encode(t, v, offset, 0);
    uint8_t *out = static_cast<uint8_t *>(std::malloc(blob.size()));
    if (!out) return nullptr;
    std::memcpy(out, blob.data(), blob.size());
    if (out_bytes) *out_bytes = static_cast<int>(blob.size());
    return out;
}
