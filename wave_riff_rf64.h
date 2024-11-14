/**
	WAVE file format.
*/

#ifndef WAVE_H
#define WAVE_H
#pragma once

// Parent class
struct Chunk {
//	char8_t  chunkId[4];
//	uint32_t chunkSize;
};

// Top of file.
typedef struct RiffChunk : Chunk {
	char8_t  chunkId[]  = 'RIFF';
	uint32_t chunkSize;                // actual size
	char8_t  riffType[] = 'WAVE';
} RiffChunk;

// Top of file.
typedef struct RF64Chunk : Chunk {
	char8_t  chunkId[]  = 'RF64';
	uint32_t chunkSize  = -1;        // -1 = 0xFFFFFFFF means don’t use this data
	char8_t  rf64Type[] = 'WAVE';
} RF64Chunk;

typedef struct ChunkSize64 {
	char8_t  chunkId[] = 'big1';	// chunk ID (“big1” – this chunk is a big one)
	uint32_t chunkSizeLow;   		// low 4 byte chunk size
	uint32_t chunkSizeHigh;			// high 4 byte chunk size
} ChunkSize64;

// Place holder chunk.
typedef struct JunkChunk : Chunk {
	char8_t  chunkId[] = 'JUNK';
	uint32_t chunkSize;        /* 4 byte size of the ‘JUNK’ chunk. This must be at least 28
								  if the chunk is intended as a place-holder for a ‘ds64’ chunk. */
	char8_t  chunkData[0];   // dummy bytes
} JunkChunk;

struct FormatChunkBase : Chunk {
	char8_t  chunkId[] = 'fmt ';
	uint32_t chunkSize;
	uint16_t formatType;        // WAVE_FORMAT_PCM = 0x0001, etc.
	uint16_t channelCount;      // 1 = mono, 2 = stereo, etc.
	uint32_t sampleRate;        // 32000, 44100, 48000, etc.
	uint32_t bytesPerSecond;    // only important for compressed formats
	uint16_t blockAlignment;    // container size (in bytes) of one set of samples
	uint16_t bitsPerSample;     // valid bits per sample 16, 20 or 24, etc.
	uint16_t cbSize;            // extra information (after cbSize) to store
};

typedef struct FormatChunk : FormatChunkBase {
//	char8_t  chunkId[4];		// 'fmt '
//	uint32_t chunkSize;
//	uint16_t formatType;        // WAVE_FORMAT_PCM = 0x0001, etc.
//	uint16_t channelCount;      // 1 = mono, 2 = stereo, etc.
//	uint32_t sampleRate;        // 32000, 44100, 48000, etc.
//	uint32_t bytesPerSecond;    // only important for compressed formats
//	uint16_t blockAlignment;    // container size (in bytes) of one set of samples
//	uint16_t bitsPerSample;     // valid bits per sample 16, 20 or 24, etc.
//	uint16_t cbSize;            // extra information (after cbSize) to store
	char8_t extraData[22];        // extra data of WAVE_FORMAT_EXTENSIBLE when necessary
} FormatChunk;

typedef struct Guid {
	uint32_t data1;
	uint16_t data2;
	uint16_t data3;
	uint32_t data4;
	uint32_t data5;
} Guid;

typedef struct FormatExtensibleChunk : FormatChunkBase {
//	char8_t  chunkId[4];		// 'fmt '
//	uint32_t chunkSize;
//	uint16_t formatType;        // WAVE_FORMAT_PCM = 0x0001, etc.
//	uint16_t channelCount;      // 1 = mono, 2 = stereo, etc.
//	uint32_t sampleRate;        // 32000, 44100, 48000, etc.
//	uint32_t bytesPerSecond;    // only important for compressed formats
//	uint16_t blockAlignment;    // container size (in bytes) of one set of samples
//	uint16_t bitsPerSample;     // valid bits per sample 16, 20 or 24, etc.
//	uint16_t cbSize;            // extra information (after cbSize) to store
	uint16_t validBitsPerSample;
	uint32_t channelMask;
	Guid
			 subFormat;    // KSDATAFORMAT_SUBTYPE_PCM, data1 = 0x00000001, data2 = 0x0000, data3 = 0x0010, data4 = 0xAA000080, data5 = 0x719B3800
} FormatExtensibleChunk;

typedef struct DataChunk : Chunk {
	char8_t  chunkId[] = 'data';
	uint32_t chunkSize;        // -1 = 0xFFFFFFFF for RF64
	char8_t  waveData[0];
} DataChunk;


typedef struct DataSize64Chunk : Chunk {
	char8_t       chunkId[] = 'ds64';
	uint32_t      chunkSize;        // 4 byte size of the ‘ds64’ chunk
	uint32_t      riffSizeLow;        // low 4 byte size of RF64 block
	uint32_t      riffSizeHigh;        // high 4 byte size of RF64 block
	uint32_t      dataSizeLow;      // low 4 byte size of data chunk
	uint32_t      dataSizeHigh;     // high 4 byte size of data chunk
	uint32_t      sampleCountLow;   // low 4 byte sample count of fact chunk
	uint32_t      sampleCountHigh;  // high 4 byte sample count of fact chunk
	uint32_t      tableLength;      // number of valid entries in array “table”
	chunkSize64_t table[0];
} DataSize64Chunk;

#endif /* WAVE_H */
