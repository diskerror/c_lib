# Digested Audio File Specifications (WAVE / AIFF / RF64)

This document unifies the specifications for WAVE, AIFF, AIFF-C, and RF64 formats to support the `AudioFile` class
implementation. It serves as the primary reference for architectural decisions, particularly regarding chunk management,
data alignment, and sample encoding.

## 1. Container Formats

Both WAVE and AIFF are chunk-based formats. The primary difference is **Endianness**.

| Feature              | WAVE / RF64                | AIFF / AIFF-C                                                                  |
|:---------------------|:---------------------------|:-------------------------------------------------------------------------------|
| **Endianness**       | **Little Endian** (Intel)  | **Big Endian** (Motorola 68k) *Exception: AIFF-C with `sowt` is Little Endian* |
| **Container ID**     | `RIFF` (or `RF64`)         | `FORM`                                                                         |
| **Form Type**        | `WAVE`                     | `AIFF` or `AIFC`                                                               |
| **Chunk Size Type**  | `uint32_t` (Little Endian) | `int32_t` (Big Endian) *See Note 1*                                            |
| **Padding Boundary** | 2 bytes (Word)             | 2 bytes (Word)                                                                 |

**Note 1 (AIFF Size):** The AIFF 1.3 specification explicitly defines the size type `long` as a **signed** 32-bit
integer, technically limiting chunk sizes to 2 GB. However, in modern practice, this is often interpreted as `uint32_t`
to support files up to 4 GB. The implementation should handle reading as unsigned to support larger files but be aware
of the signed specification when writing for strict compliance.

**CRITICAL IMPLEMENTATION DETAIL:**

- **WAVE:** Multi-byte numbers (header fields, samples) are stored LSB first.
- **AIFF:** Multi-byte numbers are stored MSB first.
- **Chunk Headers:** A chunk header is always 8 bytes: 4-byte FourCC + 4-byte Size.
- **Padding:** If a chunk's data size is odd, a single zero-byte **must** follow it. This pad byte is **NOT** included
  in the chunk's size field.

## 2. Chunk Mapping & Hierarchy

| Description       | WAVE Chunk ID   | AIFF Chunk ID     | Payload Differences                                                                   |
|:------------------|:----------------|:------------------|:--------------------------------------------------------------------------------------|
| **Format Header** | `fmt `          | `COMM`            | WAVE uses integer sample rate; AIFF uses 80-bit float (IEEE 754 extended).            |
| **Audio Data**    | `data`          | `SSND`            | `SSND` has 8 extra bytes at start: `offset` (uint32) and `blockSize` (uint32).        |
| **Peak/Levl**     | `PEAK` / `levl` | `MARK`?           | WAVE `PEAK` is non-standard but common. BWF uses `levl`.                              |
| **Markers/Cues**  | `cue `          | `MARK`            | WAVE `cue ` stores positions; AIFF `MARK` stores positions + names.                   |
| **Instrument**    | `inst`          | `INST`            | Defines looping and pitch.                                                            |
| **Sampler**       | `smpl`          | N/A               | WAVE specific sampler configuration.                                                  |
| **Metadata**      | `INFO` (List)   | `NAME`, `AUTH`... | WAVE uses a `LIST` chunk containing `INFO` sub-chunks. AIFF has discrete text chunks. |
| **Broadcast Ext** | `bext`          | N/A               | BWF (WAVE) specific. Contains origin, time, loudness, etc.                            |
| **Padding**       | `JUNK` / `PAD ` | `PAD ` / `FREE`   | Used to align subsequent chunks. Content is ignored.                                  |
| **64-bit Size**   | `ds64`          | N/A               | RF64 specific. Must be the **first** chunk after the Type ID.                         |
| **Compression**   | `fact`          | `COMM`            | AIFF-C `COMM` includes compression type. WAVE `fmt ` includes `wFormatTag`.           |
| **Format Ver**    | N/A             | `FVER`            | Required for AIFF-C.                                                                  |
| **Sound Accel**   | N/A             | `SAXL`            | AIFF-C specific.                                                                      |

## 3. High-Level Structure

### 3.1. WAVE (Standard < 4GB)

```
[RIFF] [Size] [WAVE]
    [fmt ] [Size] [WAVEFORMAT structure]
    [fact] [Size] [Sample Length] (Required for Non-PCM)
    [bext] [Size] [BWF Data...] (Optional)
    [LIST] [Size] [INFO]
        [INAM] [Size] [Title]
        [IART] [Size] [Artist]
        ...
    [data] [Size] [Interleaved Sample Data...]
```

**WAVE Format Structures:**

1. **`PCMWAVEFORMAT`**: Basic legacy structure for PCM.
2. **`WAVEFORMATEX`**: Extended structure. Adds `cbSize` (size of extra bytes). Required for non-PCM and some PCM.
3. **`WAVEFORMATEXTENSIBLE`**: Modern structure for Multichannel (>2), High Bit-depth (>16), or specific channel
   masking. Extends `WAVEFORMATEX`.
    * *Identification:* `wFormatTag` = `0xFFFE` (`WAVE_FORMAT_EXTENSIBLE`).
    * *Payload:* Includes `Samples.wValidBitsPerSample`, `dwChannelMask`, and `SubFormat` (GUID).

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

**Transition Logic:** A BWF writer often reserves space with a `JUNK` chunk at the start. If the file grows > 4GB,
`JUNK` is overwritten with `ds64`, and `RIFF` becomes `RF64`.

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

**Common AIFF-C Compression Types:**

* `NONE`: No Compression (Big Endian standard).
* `sowt`: **Little Endian** PCM (mimics WAVE byte order).
* `twos`: Big Endian PCM (Explicit standard).
* `fl32`: 32-bit Float.
* `fl64`: 64-bit Float.
* `ulaw` / `ALAW`: G.711.

## 4. Critical Alignment Requirements (User Specified)

The user requires specific alignment for the **Audio Data Payload** to optimize for disk I/O.

1. **WAVE Alignment:** The *first byte of actual sample data* inside the `data` chunk must align to a **4096-byte (4k)**
   boundary relative to the start of the file.
    * *Calculation:* `FileOffset(data_payload) % 4096 == 0`.
    * *Mechanism:* Insert a `JUNK` chunk immediately before the `data` chunk to pad.

2. **AIFF Alignment:** The *first byte of actual sample data* inside the `SSND` chunk must align to a **512-byte**
   boundary.
    * *Calculation:* `FileOffset(SSND_payload) % 512 == 0`.
    * *Note:* `SSND` payload starts *after* the 8-byte offset/blocksize header.
    * *Constraint:* The `offset` and `blockSize` fields in the `SSND` chunk must always be **zero**. (The spec
      prefers this, setting an offset value instead of using padding for alignment.)
    * *Mechanism:* Insert a `PAD ` (or `FREE`) chunk immediately before the `SSND` chunk to ensure the first byte of
      audio data aligns to the 512-byte boundary. (This makes the handling of padding the same for both file types.)

## 5. Sample Encoding

### 5.1. Data Type

Sample size is always given in number of bits, but sample size is always in increments of 8 bits. So, the most used
number of bytes is 1, 2, 3, 4, and 8.

* **8-bit:** Unsigned Integer (WAVE), Signed Integer (AIFF). **WARNING:** WAVE 8-bit is unsigned (0-255, silence=128).
  AIFF 8-bit is signed (-128 to 127, silence=0).
* **16, 24, or 32 bit:** Signed Integer (Two's Complement).
* **Float:** 32-bit or 64-bit IEEE 754 Float (-1.0 to +1.0).

### 5.2. Interleaving

* **Stereo:** [L, R, L, R, ...]
* **Multichannel:** Standard interleaved order.
    * *WAVE:* Defined by `dwChannelMask` in `WAVEFORMATEXTENSIBLE`.
    * *AIFF:* Standard mapping (L, R, C, LFE, Ls, Rs).

## 6. Metadata Preservation Rules

To prevent data loss, the `AudioFile` class must:

1. **Read:** Store all unrecognized chunks in a list of `UnknownChunk` objects.
2. **Modify:** Allow modification of `fmt`/`COMM` and `data`/`SSND`.
3. **Write:**
    * Write `ds64` (if RF64).
    * Write `fmt`/`COMM`.
    * Write `fact` (if required).
    * Write all preserved `UnknownChunk`s.
    * Calculate padding needed for alignment.
    * Write `JUNK`/`PAD ` chunk (or adjust `SSND` offset).
    * Write `data`/`SSND` chunk.

## 7. Known Metadata Chunks (To Preserve)

### 7.1. WAVE Specific

* **`bext` (Broadcast Extension):**
    * Description (256 chars), Originator (32), OriginatorRef (32), Date (10), Time (8), TimeRef (64-bit), Version,
      UMID (64), Loudness (LUFS), CodingHistory.
* **`LIST` - `INFO`:**
    * `INAM` (Name/Title), `IART` (Artist), `ICOP` (Copyright), `ICMT` (Comments), `ICRD` (Creation Date), `ISFT` (
      Software), `IENG` (Engineer), `ISRC` (Source), `ITRK` (Track), `IGNR` (Genre).
* **`cart`:** AES46 Cart Chunk (Radio traffic data).
* **`disp`:** Displayable object (icon/image).
* **`cue `:** Cue points (Markers).
* **`plst`:** Playlist (Play order of cues).
* **`adtl`:** Associated Data List (`labl`, `note`, `ltxt`).
* **`smpl`:** Sampler chunk (MIDI unity note, loops).
* **`inst`:** Instrument chunk (Gain, fine tune).
* **`dbmd`:** Dolby Metadata.
* **`levl`:** Peak Envelope.

### 7.2. AIFF Specific

* **`NAME`:** Name of sound.
* **`AUTH`:** Author.
* **`(c) `:** Copyright.
* **`ANNO`:** Annotation.
* **`COMT`:** Comment chunk (Timestamp + Marker ID + Text).
* **`MARK`:** Markers (ID + Position + Name).
* **`INST`:** Instrument (Looping, MIDI note).
* **`MIDI`:** MIDI System Exclusive data.
* **`AESD`:** Audio Recording info.
* **`APPL`:** Application specific.
* **`SAXL`:** Sound Accelerator (AIFF-C).

## 8. Chunk Placement Strategy & Direct I/O

To support efficient "direct I/O" (streaming audio directly to/from disk without loading it all into RAM), the following
rules apply:

1. **Memory Model:**
    * **Metadata:** All non-audio chunks (metadata, headers, `UnknownChunk`s) are loaded into RAM upon opening the file.
      This ensures preservation and allows for metadata modification.
    * **Audio Data:** The `data`/`SSND` payload is **NOT** loaded into RAM. It is accessed via direct disk
      seek/read/write operations.

2. **Chunk Placement:**
    * **Standard Layout:** Metadata chunks are typically placed before the `data`/`SSND` chunk to facilitate the
      critical alignment requirements (using padding chunks).
    * **Overflow:** Chunks that do not fit in the initial header block (e.g., if added after file creation) should
      generally be written **after** the `data`/`SSND` chunk to avoid shifting the massive audio payload.
    * **Rewriting:** If the audio data chunk (`data` or `SSND`) is enlarged (appended to) and there are existing chunks
      physically located after it in the file, those subsequent chunks **must be rewritten** (moved) to a new location
      after the expanded audio data. This is a potentially expensive operation but necessary to maintain file validity.

3. **In-Place Updates (Flush):**
    * The `flush()` operation primarily performs an in-place update of the Chunk Size fields (Header size and Data size)
      on disk.
    * It does **not** automatically handle complex chunk moves or insertions in the "Direct I/O" mode. Users (or
      higher-level logic) must handle file reconstruction if chunks need to be inserted or moved.

## 9. Implementation Architecture & Class Responsibilities

### 9.1. AudioFile Class

* **Scope:** Manages the file container structure and chunk list.
* **Parsing:**
    * Reads the first 12 bytes of the file to determine type (`RIFF`/`WAVE`, `FORM`/`AIFF`, etc.).
    * Iterates through the file, reading only the first 8 bytes of each chunk (Header: ID + Size) to build a map of
      chunks.
    * **Exception:** For AIFF `SSND` chunks, it reads the first 16 bytes (Header + 4-byte Offset + 4-byte BlockSize) to
      validate they are zero (as per alignment spec).
* **Memory:** Loads **all** chunks into RAM *except* the audio data payload (`data` or `SSND` content). Audio data
  remains on disk.
* **Ordering:** Preserves the original order of chunks as found in the file.
* **Stream Behavior:**
    * Inherits from or behaves like `std::fstream`.
    * **Constraint:** Read and write pointers are constrained to the start and end of the audio payload within the
      `data`/`SSND` chunk.
    * **Tracking:** Must track the absolute file offsets of the audio data start and end to enforce these constraints.
    * **Chunk I/O:** Internal chunk reading/writing operations must manage the file pointer carefully to avoid
      disrupting the user's audio stream position, or restore it afterwards.
* **Chunk Management:**
    * Supports multiple chunks of the same type (e.g., multiple `ANNO` chunks in AIFF).
    * Provides methods to retrieve and set chunks (both solo and multiple).
    * **Responsibility:** Reads, organizes, and writes chunks. Does **not** interpret chunk content (except for basic
      header validation and size endianness). Chunks will have their entire structure managed by the user, including 
      endianness, and including the chunk ID and size.
    * **Endianness:** Selects the correct endianness for file I/O based on the file type (Little Endian for WAVE, Big
      Endian for AIFF).
* **Chunk Enumeration:**
    * Provides a method to retrieve a list of all chunks present in the file.
    * **Format:** A collection (e.g., `std::vector`) of triples containing:
        1. **ID:** Chunk ID (FourCC).
        2. **Size:** Data size in native endianness.
        3. **Index:** The zero-based sequential index of the chunk in the file's chunk list. This index can be used to
           retrieve the specific chunk instance.
* **File Creation:**
    * `AudioFormat` provides a properly crafted format chunk to `AudioFile`.
    * `AudioFile` handles the initial 12-byte container header and the audio data chunk structure.
* **Flush:**
    * Updates total file size in the container header.
    * Updates audio data chunk size.
    * Ensures all chunks are written to disk.

### 9.2. AudioFormat Class

* **Scope:** Handles specific audio encoding details (Bit Depth, Sample Rate, Encoding, Channel Mask).
* **Responsibility:**
    * Retrieves the appropriate format chunk from the `AudioFile`'s chunk list based on the base file type (`fmt ` for
      WAVE, `COMM` for AIFF).
    * Parses the payload of the format chunk to determine audio characteristics.
    * **Transparency:** Internals are transparent to the user. Users get/set values (e.g., `setSampleRate(44100)`), and
      `AudioFormat` ensures the correct chunk structure is generated and sent to `AudioFile`.
    * **Chunk Creation:** Responsible for creating the format chunk for new files, ensuring correct endianness for the
      target file type.

### 9.3. Constraints & Limitations

* **Format Immutability:**
    * **Existing Files:** The format chunk (`fmt ` or `COMM`) **cannot** be changed.
    * **New Files:** The format **cannot** be changed once the format chunk or any audio data has been written to disk.
* **Unsupported Formats:**
    * **RF64:** Read-only support. Creation or update of RF64 files is **not** supported.
    * **RIFX:** Big-endian WAVE (RIFX) is **not** supported.

## 10. Implementation Details: Endianness & Portability

To ensure cross-platform compatibility (compilable on both Big Endian and Little Endian systems), the implementation
must adhere to the following rules:

1. **Library:** Use the `boost/endian` library for all file I/O operations.
2. **Data Types:** All class members and structure properties representing data written to or read from disk must use
   specific endian types from `boost::endian` (e.g., `boost::endian::little_int32_t`, `boost::endian::big_uint16_t`).
    * **WAVE/RF64:** Use Little Endian types.
    * **AIFF:** Use Big Endian types.
3. **Native Types:** If a `boost/endian` type labeled "native" is used, or if standard C++ types (e.g., `int`, `float`)
   are used without wrappers, they are assumed to be in the host system's native endianness. These should generally be
   restricted to in-memory logic, not direct file serialization, unless explicitly converted.
4. **FourCC Handling:** Four-Character Codes (FourCC) must be stored using `boost::endian::big_uint32_t` (or equivalent)
   to ensure they appear correctly in memory and on disk when assigned from character literals (e.g., `'COMM'`). This
   ensures readability in the code while maintaining binary compatibility.
5. **Chunk Data:** The user (including `AudioFormat`) is responsible for the endianness of all data within a chunk,
   including the ID and Size fields. `AudioFile` treats chunk data as opaque bytes.
