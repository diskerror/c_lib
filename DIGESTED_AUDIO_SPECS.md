# Digested Audio File Specifications (WAVE / AIFF / RF64)

This document unifies the specifications for WAVE, AIFF, AIFF-C, and RF64 formats to support the `AudioFile` class implementation. It serves as the primary reference for architectural decisions, particularly regarding chunk management, data alignment, and sample encoding.

## 1. Container Formats

Both WAVE and AIFF are chunk-based formats. The primary difference is **Endianness**.

| Feature | WAVE / RF64 | AIFF / AIFF-C |
| :--- | :--- | :--- |
| **Endianness** | **Little Endian** (Intel) | **Big Endian** (Motorola 68k) *Exception: AIFF-C with `sowt` is Little Endian* |
| **Container ID** | `RIFF` (or `RF64`) | `FORM` |
| **Form Type** | `WAVE` | `AIFF` or `AIFC` |
| **Chunk Size Type** | `uint32_t` (Little Endian) | `uint32_t` (Big Endian) |
| **Padding Boundary** | 2 bytes (Word) | 2 bytes (Word) |

**CRITICAL IMPLEMENTATION DETAIL:**
- **WAVE:** Multi-byte numbers (header fields, samples) are stored LSB first.
- **AIFF:** Multi-byte numbers are stored MSB first.
- **Chunk Headers:** A chunk header is always 8 bytes: 4-byte FourCC + 4-byte Size.
- **Padding:** If a chunk's data size is odd, a single zero-byte **must** follow it. This pad byte is **NOT** included in the chunk's size field.

## 2. Chunk Mapping & Hierarchy

| Description | WAVE Chunk ID | AIFF Chunk ID | Payload Differences |
| :--- | :--- | :--- | :--- |
| **Format Header** | `fmt ` | `COMM` | WAVE uses integer sample rate; AIFF uses 80-bit float (IEEE 754 extended). |
| **Audio Data** | `data` | `SSND` | `SSND` has 8 extra bytes at start: `offset` (uint32) and `blockSize` (uint32). |
| **Peak/Levl** | `PEAK` / `levl` | `MARK`? | WAVE `PEAK` is non-standard but common. |
| **Markers/Cues** | `cue ` | `MARK` | |
| **Instrument** | `inst` | `INST` | |
| **Metadata** | `INFO` (List) | `NAME`, `AUTH`... | WAVE uses a `LIST` chunk containing `INFO` sub-chunks. AIFF has discrete text chunks. |
| **Broadcast Ext** | `bext` | `bext`? | Typically BWF (WAVE) only, but can technically exist in AIFF. |
| **Padding** | `JUNK` | `PAD ` | Used to align subsequent chunks. Content is ignored. |
| **64-bit Size** | `ds64` | N/A | RF64 specific. Must be the **first** chunk after the Type ID. |
| **Compression** | `fact` | `COMM` | AIFF-C `COMM` includes compression type (FourCC). WAVE `fmt ` includes `wFormatTag`. |

## 3. High-Level Structure

### 3.1. WAVE (Standard < 4GB)
```
[RIFF] [Size] [WAVE]
    [fmt ] [Size] [WAVEFORMATEX struct]
    [bext] [Size] [BWF Data...] (Optional)
    [fact] [Size] [Sample Length...] (Required for Compressed/Float)
    [data] [Size] [Interleaved Sample Data...]
```

### 3.2. RF64 (Extended > 4GB)
```
[RF64] [0xFFFFFFFF] [WAVE]
    [ds64] [Size] 
        [64-bit RIFF Size]
        [64-bit Data Chunk Size]
        [64-bit Sample Count]
        [Table of other 64-bit chunks...]
    [fmt ] ...
    [data] [0xFFFFFFFF] [Interleaved Sample Data...]
```
**Transition Logic:** A BWF writer often reserves space with a `JUNK` chunk at the start. If the file grows > 4GB, `JUNK` is overwritten with `ds64`, and `RIFF` becomes `RF64`.

### 3.3. AIFF / AIFF-C
```
[FORM] [Size] [AIFF] (or AIFC)
    [FVER] [Size] [Timestamp] (Required for AIFC)
    [COMM] [Size] 
        [NumChannels]
        [NumSampleFrames]
        [SampleSize]
        [SampleRate (80-bit Float)]
        [CompressionID] (AIFC only - 4 chars)
        [CompressionName] (AIFC only - Pascal String)
    [SSND] [Size]
        [Offset (usually 0)]
        [BlockSize (usually 0)]
        [Interleaved Sample Data...]
```

**Common AIFF-C Compression Types (from mpruett/audiofile):**
*   `NONE`: No Compression (Big Endian standard).
*   `sowt`: **Little Endian** PCM (mimics WAVE byte order). Technically "uncompressed" but byte-swapped relative to standard AIFF.
*   `twos`: Big Endian PCM (Explicit standard).
*   `fl32` / `FL32`: 32-bit Float.
*   `fl64` / `FL64`: 64-bit Float.
*   `ulaw` / `ULAW`: µ-law.
*   `alaw` / `ALAW`: A-law.

## 4. Critical Alignment Requirements (User Specified)

The user requires specific alignment for the **Audio Data Payload** to optimize for disk I/O.

1.  **WAVE Alignment:** The *first byte of actual sample data* inside the `data` chunk must align to a **4096-byte (4k)** boundary relative to the start of the file.
    *   *Calculation:* `FileOffset(data_payload) % 4096 == 0`.
    *   *Mechanism:* Insert a `JUNK` chunk immediately before the `data` chunk to pad.

2.  **AIFF Alignment:** The *first byte of actual sample data* inside the `SSND` chunk must align to a **512-byte** boundary.
    *   *Calculation:* `FileOffset(SSND_payload) % 512 == 0`.
    *   *Note:* `SSND` payload starts *after* the 8-byte offset/blocksize header.
    *   *Mechanism:* Insert a `PAD ` chunk (or `FREE`) immediately before the `SSND` chunk.

## 5. Sample Encoding

### 5.1. Data Type
*   **1-8 bit:** Unsigned Integer (WAVE), Signed Integer (AIFF). **WARNING:** WAVE 8-bit is unsigned (0-255, silence=128). AIFF 8-bit is signed (-128 to 127, silence=0).
*   **9-32 bit:** Signed Integer (Two's Complement).
*   **Float:** 32-bit or 64-bit IEEE 754 Float (-1.0 to +1.0).

### 5.2. Interleaving
*   **Stereo:** [L, R, L, R, ...]
*   **Multichannel:** Standard interleaved order (e.g., L, R, C, LFE, Ls, Rs).

## 6. Metadata Preservation Rules

To prevent data loss, the `AudioFile` class must:
1.  **Read:** Store all unrecognized chunks in a list of `UnknownChunk` objects.
2.  **Modify:** Allow modification of `fmt`/`COMM` and `data`/`SSND`.
3.  **Write:** 
    *   Write `ds64` (if RF64).
    *   Write `fmt`/`COMM`.
    *   Write all preserved `UnknownChunk`s.
    *   Calculate padding needed for alignment.
    *   Write `JUNK`/`PAD ` chunk.
    *   Write `data`/`SSND` chunk.

## 7. Known Metadata Chunks (To Preserve)
*Reference: https://github.com/mpruett/audiofile/*

-   `bext` (Broadcast Extension)
-   `iXML` (XML Metadata)
-   `dbmd` (Dolby Metadata)
-   `cart` (AES Cart Chunk)
-   `DISP` / `disp` (Display/Icon)
-   `LIST` - `INFO` (WAVE Standard Metadata: Artist, Copyright, etc.)
-   `ID3 ` (ID3v2 Tags)
-   `levl` (Peak Levels)
-   **AIFF Specific:**
    -   `AESD` (AES Recording Data)
    -   `ANNO` (Annotation)
    -   `APPL` (Application Specific)
    -   `MIDI` (MIDI Data)
    -   `NAME`, `AUTH`, `(c) ` (Copyright)
-   **WAVE Specific:**
    -   `plst` (Playlist)
    -   `adtl` (Associated Data List: `labl`, `note`)

## 8. Chunk Placement Strategy & Direct I/O

To support efficient "direct I/O" (streaming audio directly to/from disk without loading it all into RAM), the following rules apply:

1.  **Memory Model:**
    *   **Metadata:** All non-audio chunks (metadata, headers, `UnknownChunk`s) are loaded into RAM upon opening the file. This ensures preservation and allows for metadata modification.
    *   **Audio Data:** The `data`/`SSND` payload is **NOT** loaded into RAM. It is accessed via direct disk seek/read/write operations.

2.  **Chunk Placement:**
    *   **Standard Layout:** Metadata chunks are typically placed before the `data`/`SSND` chunk to facilitate the critical alignment requirements (using padding chunks).
    *   **Overflow:** Chunks that do not fit in the initial header block (e.g., if added after file creation) should generally be written **after** the `data`/`SSND` chunk to avoid shifting the massive audio payload.
    *   **Rewriting:** If the audio data chunk (`data` or `SSND`) is enlarged (appended to) and there are existing chunks physically located after it in the file, those subsequent chunks **must be rewritten** (moved) to a new location after the expanded audio data. This is a potentially expensive operation but necessary to maintain file validity.

3.  **In-Place Updates (Flush):**
    *   The `flush()` operation primarily performs an in-place update of the Chunk Size fields (Header size and Data size) on disk.
    *   It does **not** automatically handle complex chunk moves or insertions in the "Direct I/O" mode. Users (or higher-level logic) must handle file reconstruction if chunks need to be inserted or moved.
