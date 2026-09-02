# Vector storage dtypes (on-disk embedding precision)

`EmbeddingCodec` (`EmbeddingCodec.h`/`.cp`, namespace `Diskerror::EmbeddingCodec`)
packs/unpacks float32 embedding vectors into compact on-disk BLOB
representations. All in-memory math stays f32; this layer only governs the
stored bytes. Consumers (Ragger, SemanticSQLite) vendor this via CMake
`FetchContent` and link `diskerror_embedding_codec`.

## Supported types

| dtype  | bytes/dim | layout | notes |
|--------|-----------|--------|-------|
| `f32`  | 4 | IEEE 754 single | Lossless baseline. |
| `f16`  | 2 | IEEE half (1s/5e/10m) | More mantissa than bf16; smaller exponent range (max ≈ 65504). Fine for normalized embeddings. |
| `bf16` | 2 | bfloat16 (1s/8e/7m) | Same exponent range as f32, fewer mantissa bits. Round-to-nearest-even, NaN preserved. Useful only if a model produces large-magnitude components that f16 would overflow. |
| `int8` | 1 | symmetric per-vector | `scale = max\|x\| / 127`, stored as a 2-byte f16 suffix after the payload. 4× smaller than f32, 2× smaller than f16. |

`f16` and `bf16` are **not** interchangeable despite both being 2 bytes/dim —
the blob itself carries no dtype tag, so the caller must know which dtype it
used to encode before it can decode.

## Blob layout and the `offset` parameter

`EmbeddingCodec` has no fixed header of its own. Every blob is:

```
[0, offset)        caller-defined header/prefix bytes (untouched by the codec,
                   except encode() optionally stamps byte 0 with `version`
                   when offset >= 1)
[offset, ...)      payload: dims × payload_stride(t) bytes

INT8 only, appended right after the float payload above:
[offset+dims, +2)  scale (float16, symmetric dequant multiplier)
```

`offset` is a plain caller-supplied byte count, not something the codec
infers or enforces beyond validating the blob is big enough. It exists
because different consumers put different things (or nothing) before the
payload:

- **Ragger (db_version 0.15+)** stores payload-only blobs — `offset = 0`.
  The embedding version tag lives in its own `embedding_version` column
  instead of inside the blob.
- **Legacy Ragger blobs (pre-0.15)** prefixed a single version byte before
  the payload — `offset = 1`. `encode(t, v, 1, version)` writes that byte;
  `decode(..., offset=1)` skips it before reading the payload.
  `blob_version()` reads byte 0 directly for this case.
- **SemanticSQLite** lets the user configure an arbitrary start offset per
  database when examining embedding blobs that carry their own header before
  the payload (a value the user determines by inspecting that DB — see its
  README for the copy-paste example deriving `offset` from settings).

Callers pass the same `offset` to both `encode()` and `decode()`/
`expected_blob_size()` for a given blob; there's no auto-detection.

## Public API

```cpp
int payload_stride(VectorType t);                     // bytes/dim of the payload
int payload_size(VectorType t, int dims);              // payload bytes, no offset
int expected_blob_size(VectorType t, int dims, int offset = 0);
                                                        // offset + payload_size(...)

std::vector<uint8_t> encode(VectorType t, const std::vector<float>& v,
                            int offset = 0, uint8_t version = 0);
bool decode(const void* blob, int blob_bytes, int expected_dims,
            VectorType t, std::vector<float>& out, int offset = 0);

int blob_version(const void* blob, int blob_bytes);    // reads blob[0]; only
                                                        // meaningful when offset >= 1
```

`decode()` validity requires `blob_bytes > offset` (there must be bytes
beyond the header) **and** `blob_bytes - offset == payload_size(t, dims)`
(the payload region is exactly the right size). On failure (null, size
mismatch, offset too large) `out` is zero-filled and `false` is returned.

## Changing the stored precision (Ragger)

`vector_type` is part of a Ragger DB's vector identity (model + dims +
dtype). To change it on a DB that already holds data:

1. Set the desired type (dashboard "Desired Vector Type", or
   `desired_embedding_vector_type` in config / the `settings` table).
2. Run `ragger rebuild-embeddings` (or "Update now" in the dashboard) to
   re-encode every stored vector at the new precision.

A drift guard refuses to run a daemon whose stored `vector_type` disagrees
with config until you rebuild, so you can't silently mix precisions in one
DB.

## Benchmark: recall vs precision on real data

`scripts/bench_vector_quant.py` (in the Ragger repo) measures how each dtype
perturbs nearest-neighbor rankings versus full precision, using the actual
embeddings in a `memories.db`. Recall@k = "of the k nearest neighbors under
full precision, how many still appear in the top-k under the quantized
store" (1.0 = identical ranking).

Run on an 18,623-vector corpus (384-dim `all-MiniLM-L6-v2`, 400 sampled
queries). **Note:** this DB was already stored as f16, so the f32/f16 rows
are trivially 1.0 (upcasting f16→f32 is lossless); the meaningful signal is
bf16 and int8 measured relative to that f16 store.

| dtype  | size    | recall@1 | recall@5 | recall@10 | recall@20 | mean cos err |
|--------|---------|----------|----------|-----------|-----------|--------------|
| f32    | 27.9 MB | 1.0000   | 1.0000   | 1.0000    | 1.0000    | 5.3e-08 |
| f16    | 14.0 MB | 1.0000   | 1.0000   | 1.0000    | 1.0000    | 5.3e-08 |
| bf16   | 14.0 MB | 1.0000   | 0.9895   | 0.9830    | 0.9925    | 1.4e-06 |
| int8   |  7.0 MB | 0.9925   | 0.9875   | 0.9785    | 0.9878    | 4.3e-05 |

### Takeaways for `all-MiniLM-L6-v2` (the default model)

- **f16 (default) is the right default.** For this normalized 384-dim model
  it is effectively lossless vs f32 at half the size — no overflow issues,
  no measurable recall loss.
- **bf16 is strictly worse than f16 here.** Same size, lower recall — its
  extra exponent range buys nothing because MiniLM components are already
  small (`|x| < 1`). Only reach for bf16 with a model that emits
  large-magnitude components f16 would overflow (rare for normalized
  embeddings).
- **int8 gives 4× compression vs f32 (2× vs f16) for ~1–2% recall loss.**
  Attractive when the corpus grows large enough that the in-memory matrix or
  startup load time matters, and a couple points of recall@10 is acceptable.
  At small corpus sizes there's no reason to bother — f16 already fits.
- **Subject matter matters.** These numbers are model- and corpus-specific.
  Re-run `bench_vector_quant.py` against your own DB before switching a
  large or accuracy-critical store to int8; a domain with tightly clustered
  embeddings can lose more recall to quantization than this general corpus
  did.

## Sub-byte formats (int4, binary) — evaluated, not shipped

`bench_vector_quant.py` also simulates int4 (0.5 B/dim, 15 levels,
per-vector scale) and binary (1 bit/dim = 0.125 B/dim, sign quantization /
Hamming). These are **not implemented in `EmbeddingCodec`** — the benchmark
measures them in Python to decide whether they're worth building. They are
not.

**Raw recall@10** (rank entirely on the stored dtype, 18.6k MiniLM vectors):

| dtype  | size    | recall@10 | mean cos err |
|--------|---------|-----------|--------------|
| int8   | 7.0 MB  | 0.9770    | 4.3e-05 |
| int4   | 3.5 MB  | 0.8810    | 1.4e-02 |
| binary | 0.9 MB  | 0.6332    | 2.1e-01 |

int4 loses ~12 points and binary ~37 points of recall@10 — far too lossy to
rank on directly. In practice low-bit indexes are always used **two-stage**:
fetch `oversample × k` candidates cheaply on the small vectors, then rerank
that shortlist with full-precision f32. With oversample ×4 / ×8 (f32 rerank;
the ~0.985 ceiling is a duplicate-vector tie-breaking artifact, not real
loss):

| dtype  | raw    | rerank ×4 | rerank ×8 |
|--------|--------|-----------|-----------|
| int8   | 0.9770 | 0.9878    | 0.9858 |
| int4   | 0.8810 | 0.9833    | 0.9808 |
| binary | 0.6332 | 0.9028    | 0.9485 |

**Why they're not worth building:**

- The two-stage recovery **requires keeping f32 (or f16) vectors around to
  rerank with.** That defeats the storage win — you'd store *both* the tiny
  vectors and the full-precision set. It only pays off when the low-bit
  index lives in RAM and the f32 set lives on slower/cheaper storage, or is
  recomputed on demand. Ragger keeps one in-memory `Eigen::MatrixXf` and
  does a brute-force cosine scan; there is no separate ANN index for a
  binary first-stage to accelerate, and no second stage to rerank into.
- At Ragger's corpus scale (thousands to low tens-of-thousands of vectors)
  the f16 matrix is already small (~14 MB here) and the scan is
  sub-millisecond. The problem sub-byte formats solve — RAM pressure and
  scan cost on 100M-vector indexes — isn't one Ragger has.
- int8 already captures the useful part of the curve: 4× smaller than f32
  for ~2% recall, no rerank stage needed. Below that, you pay real accuracy
  or real architectural complexity (a two-tier retrieve+rerank pipeline) for
  space Ragger isn't short on.

If a consumer ever grows a true ANN index (HNSW/IVF) over a much larger
corpus, revisit binary-first + f32-rerank then — that's the regime where it
wins. Until there's telemetry showing the f16 scan is a bottleneck, it's
speculative complexity. Re-run the benchmark to regenerate these numbers on
any corpus.

## Why int8 works: embedding distribution and cosine geometry

**Embedding value distribution.** For mean-pooled, L2-normalized embeddings
(the kind Ragger produces), individual components are approximately
Gaussian and symmetric around zero:

- Mean ≈ 0 (within ±0.01)
- Std dev ≈ 1/√dims — e.g., ~0.051 for 384-dim, ~0.036 for 768-dim
- Typical range −0.15 to +0.15 with most values between −0.05 and +0.05

This symmetry is why simple max-abs int8 quantization works: a single scale
factor (`max|x| / 127`) maps the distribution cleanly onto [−127, +127]
without needing an offset/zero-point. Zero-point becomes relevant only for
asymmetric ranges (uint8 [0,255], or non-symmetric float embeddings).

**Cosine similarity is scale-invariant.** `cos(θ) = A·B / (|A||B|)` —
multiply either vector by any scalar and the result doesn't change. Only
direction/shape matters, not absolute magnitude. This has two practical
consequences:

1. **int8 quantization preserves what matters.** Rounding each dimension
   independently keeps relative proportions within a vector intact; the
   overall direction is maintained even though individual values are
   noisier.
2. **Binary embeddings fail for a specific reason.** Not because they lose
   absolute values (cosine doesn't care about those) but because they
   destroy *relative* proportions — every dim becomes ±1 regardless of
   original magnitude, so you can't distinguish "slightly positive" from
   "very positive."

**Int8 rounding error math.** Per-dim step size = 2/127 ≈ 0.0157; uniform
rounding error std dev = step/√12 ≈ 0.0045. Norm drift accumulates in
quadrature across dims:

```
E[|v_quantized|²] = |v_original|² + N × σ_error²
```

- 384-dim int8: `|v| ≈ √(1 + 0.0078) ≈ 1.0039` (0.39% drift)
- 768-dim int8: `|v| ≈ √(1 + 0.0156) ≈ 1.0078` (0.78% drift)

But cosine similarity barely cares about this norm drift because rounding
errors are mostly *orthogonal* to the original vector — they add noise
perpendicular to the signal rather than along it. Typical angular error for
int8 at these dims is ~0.5–1°, giving cosine sim to the original around
0.999+. This explains why skipping renormalization costs <0.002 recall@10
on int8 despite the norm drift: the direction is preserved well enough that
the magnitude correction barely matters for ranking purposes.

**Renormalize-on-load.** Still worth doing because (a) it corrects the small
norm drift so cosine comparisons operate on vectors as close to unit-norm as
possible, and (b) floating-point precision in `dot/(|a||b|)` means dividing
by a slightly wrong denominator. Cost is one sqrt + one division per vector
at load — negligible compared to embedding computation. For f16/bf16 the
drift is tiny enough that renormalizing barely matters; for int8 it's worth
doing because the rounding error is larger.

## 768-dim/int8 vs 384-dim/f16 — same byte count, different tradeoff

Same total data volume (768×8 = 384×16 bits) but fundamentally different:

- **More dims = more information capacity.** A 768-dim vector can represent
  finer distinctions in the embedding space.
- **Lower precision per dim = more quantization noise.** int8 has ~256
  levels vs f16's ~65k, so each dimension is noisier.

For most modern embedding models (BGE, E5, GTE), dimensions matter more than
precision because the model was trained in that dimensional space and can't
recover lost dims from higher precision. A 768-dim int8 vector preserves all
structural info the model produced; a 384-dim f16 has perfect precision on
only half the dims.

**Caveats:** you'd need to re-embed everything with the larger model, and
query embeddings must come from the same model (different models produce
incompatible vector spaces). Worth benchmarking with `bench_vector_quant.py`
— at Ragger's corpus scale (~18k vectors) the difference is measurable but
not dramatic.
