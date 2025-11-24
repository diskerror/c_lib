/**
 *	WAVE file format.
 */

#ifndef DISKERROR_WAVE_H
#define DISKERROR_WAVE_H
#pragma once

#include <boost/endian/arithmetic.hpp>
using namespace boost::endian;

//	Four-character code.
#ifndef HAS_FOURCC_T
#define HAS_FOURCC_T
typedef big_uint32_t fourcc_t;
#endif

#define SPEAKER_FRONT_LEFT 0x00000001
#define SPEAKER_FRONT_RIGHT 0x00000002
#define SPEAKER_FRONT_CENTER 0x00000004
#define SPEAKER_LOW_FREQUENCY 0x00000008
#define SPEAKER_BACK_LEFT 0x00000010
#define SPEAKER_BACK_RIGHT 0x00000020
#define SPEAKER_FRONT_LEFT_OF_CENTER 0x00000040
#define SPEAKER_FRONT_RIGHT_OF_CENTER 0x00000080
#define SPEAKER_BACK_CENTER 0x00000100
#define SPEAKER_SIDE_LEFT 0x00000200
#define SPEAKER_SIDE_RIGHT 0x00000400
#define SPEAKER_TOP_CENTER 0x00000800
#define SPEAKER_TOP_FRONT_LEFT 0x00001000
#define SPEAKER_TOP_FRONT_CENTER 0x00002000
#define SPEAKER_TOP_FRONT_RIGHT 0x00004000
#define SPEAKER_TOP_BACK_LEFT 0x00008000
#define SPEAKER_TOP_BACK_CENTER 0x00010000
#define SPEAKER_TOP_BACK_RIGHT 0x00020000
#define SPEAKER_ALL 0x80000000

#define SPEAKER_STEREO_LEFT 0x20000000
#define SPEAKER_STEREO_RIGHT 0x40000000

#define SPEAKER_CONTROLSAMPLE_1 0x08000000
#define SPEAKER_CONTROLSAMPLE_2 0x10000000

#define SPEAKER_BITSTREAM_1_LEFT 0x00800000
#define SPEAKER_BITSTREAM_1_RIGHT 0x01000000
#define SPEAKER_BITSTREAM_2_LEFT 0x02000000
#define SPEAKER_BITSTREAM_2_RIGHT 0x04000000

typedef struct WaveFileHeader {
	fourcc_t        id   = '    '; //	RIFF or RF64 (or RIFX, not yet supported)
	little_uint32_t size = 0;      //	file total size - 8
	fourcc_t        type = '    '; //	WAVE
} waveFileHeader_t;

// Parent class
typedef struct {
	fourcc_t        id;
	little_uint32_t size;
} waveChunkHead_t;


//	Data 'data' and Junk 'JUNK' Chunks
typedef struct WaveDataChunk : waveChunkHead_t {
	unsigned char data[];
} waveData_t;


//	Format data structure.
//	Chunk data with type 'fmt ' and size 16 will have this structure.
typedef struct FormatData : waveChunkHead_t {
	little_uint16_t type;           // WAVE_FORMAT_PCM = 0x0001, etc.
	little_uint16_t channelCount;   // 1 = mono, 2 = stereo, etc.
	little_uint32_t sampleRate;     // 32000, 44100, 48000, etc.
	little_uint32_t bytesPerSecond; // average, only important for compressed formats
	little_uint16_t blockAlignment; // container size (in bytes) of one set of samples
	little_uint16_t bitsPerSample;  // valid bits per sample 16, 20 or 24, etc.
} FormatData_t;

//	Chunk data with type 'fmt ' and size 18 will have this structure.
typedef struct FormatPlusData : FormatData {
//	little_uint16_t type;              // WAVE_FORMAT_PCM = 0x0001, etc.
//	little_uint16_t channelCount;      // 1 = mono, 2 = stereo, etc.
//	little_uint32_t sampleRate;        // 32000, 44100, 48000, etc.
//	little_uint32_t bytesPerSecond;    // average, only important for compressed formats
//	little_uint16_t blockAlignment;    // container size (in bytes) of one set of samples
//	little_uint16_t bitsPerSample;     // valid bits per sample 16, 20 or 24, etc.
	little_uint16_t cbSize; // extra information (after cbSize) to store
} FormatPlusData_t;

// typedef struct _GUID {
//   unsigned long  Data1;
//   unsigned short Data2;
//   unsigned short Data3;
//   unsigned char  Data4[8];
// } GUID;
typedef struct GUID {
	little_uint32_t Data1;
	little_uint16_t Data2;
	little_uint16_t Data3;
	big_uint32_t    Data4;
	big_uint32_t    Data5;
} GUID_t;

// // For a WAVE_FORMAT_X tag = 0xXYZW the WAVEFORMATEXTENSIBLE SubType GUID is
// // 0000XYZW-0000-0010-8000-00AA00389B71
// #define DEFINE_WAVEFORMATEX_GUID(x) (USHORT)(x), 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71
//	????

//	Chunk data with type 'fmt ' and size 40 will have this structure.
typedef struct FormatExtensibleData : FormatPlusData {
//	little_uint16_t type;              // WAVE_FORMAT_PCM = 0x0001, etc.
//	little_uint16_t channelCount;      // 1 = mono, 2 = stereo, etc.
//	little_uint32_t sampleRate;        // 32000, 44100, 48000, etc.
//	little_uint32_t bytesPerSecond;    // average, only important for compressed formats
//	little_uint16_t blockAlignment;    // container size (in bytes) of one set of samples
//	little_uint16_t bitsPerSample;     // valid bits per sample 16, 20 or 24, etc.
//	little_uint16_t cbSize;            // extra information (after cbSize) to store
	union {
		little_uint16_t wValidBitsPerSample; /* bits of precision  */
		little_uint16_t wSamplesPerBlock;    /* valid if wBitsPerSample==0 */
		little_uint16_t wReserved;           /* If neither applies, set to zero. */
	} Samples;

	little_uint32_t channelMask;
	GUID_t          subFormat;
} FormatExtensibleData_t, fmt_t;

//	Describes Broadcast Audio Extension data structure.
//		id = 'bext'
//		size >= 602
typedef struct BroadcastAudioExtension : waveChunkHead_t {
	char            Description[256]; // ASCII : Description of the sound sequence
	char            Originator[32]; // ASCII : Name of the originator
	char            OriginatorReference[32]; // ASCII : Reference of the originator
	char            OriginationDate[10]; // ASCII : yyyy:mm:dd
	char            OriginationTime[8]; // ASCII : hh:mm:ss
	little_int64_t  TimeReference; // First sample count since midnight
	little_int16_t  Version; // Version of the BWF; unsigned binary number
	uint8_t         UMID[64]; // Binary SMPTE UMID
	little_int16_t  LoudnessValue; // Integrated Loudness Value of the file in LUFS x100
	little_uint16_t LoudnessRange; // Maximum True Peak Level of the file expressed as dBTP x100
	little_int16_t  MaxTruePeakLevel; // Maximum True Peak Level of the file expressed as dBTP x100
	little_int16_t  MaxMomentaryLoudness; // Highest value of the Momentary Loudness Level of the file in LUFS (x100)
	little_int16_t  MaxShortTermLoudness; // Highest value of the Short-Term Loudness Level of the file in LUFS (x100)
	uint8_t         Reserved[180]; // Reserved for future use, set to NULL
	char            CodingHistory[0]; // ASCII : History coding
} BroadcastAudioExt_t, bext_t;

//	RF64 Size Chunk: id = 'big1' (this chunk is a big one)
//		size of data in Data Chunk
typedef struct {
	fourcc_t       id;
	little_int64_t size;
} ChunkSize64_t, big1_t;

//	Describes size of data in 'data' chunk.
//		id = 'ds64'
//		size >= 28
typedef struct DataSize64Chunk : waveChunkHead_t {
	little_int64_t riffSize;    // size of RF64 block (file size - 8)
	little_int64_t dataSize;    // size of data chunk
	little_int64_t sampleCount; // sample count of fact chunk (frames)
	little_int32_t tableLength; // number of valid entries in array “table”
	ChunkSize64_t  table[];
} DataSize64_t, ds64_t;

//	Fact Chunk 'fact'
//	(compressed audio, number of samples [frames] after decompression)
typedef struct FactChunk : waveChunkHead_t {
	little_uint32_t samples;
} fact_t;

//	Cue Chunk 'cue '
typedef little_uint32_t cueID_t;

typedef struct {
	cueID_t         identifier;
	little_uint32_t position;
	fourcc_t        fccChunk;	 //	'data'
	little_uint32_t chunkStart;
	little_uint32_t blockStart;
	little_uint32_t sampleOffset;
} CuePoint_t;

typedef struct CueChunk : waveChunkHead_t {
	little_uint32_t length;
	CuePoint_t      point[];
} cue_t;

//	Playlist chunk 'plst'
typedef struct {
	cueID_t         identifier;
	little_uint32_t length;
	little_uint32_t repeats;
} segment_t;

typedef struct PlaylistChunk : waveChunkHead_t {
	little_uint32_t length;
	segment_t       segment[];
} plst_t;

//	Associated Data List 'list' type 'adtl':
//	Label Chunk 'labl', Note Chunk 'note' inside List
typedef struct LabelNoteChunk : waveChunkHead_t {
	cueID_t identifier;
	char    text[]; //	null-terminated
} lablnote_t;

//	Labeled Text Chunk 'ltxt' inside List
typedef struct LabeledTextChunk : waveChunkHead_t {
	cueID_t         identifier;
	little_uint32_t length;
	little_uint32_t purpose;
	little_uint16_t country;
	little_uint16_t language;
	little_uint16_t dialect;
	little_uint16_t codePage;
	char            text[]; //	null-terminated
} ltxt_t;

typedef struct AssiciatedDataList : waveChunkHead_t {
//	fourcc_t        id;	/* 'list' */
//	little_uint32_t size;
	fourcc_t typeID; /* 'adtl' */
	union {
		fourcc_t   sub_id;
		lablnote_t labl[];
		lablnote_t note[];
		ltxt_t     ltxt[];
	};
} listHead_t;

//	Sampler Chunk 'smpl'
typedef little_uint32_t samplerID_t;

typedef struct {
	samplerID_t     identifier;
	little_uint32_t type;
	little_uint32_t start;
	little_uint32_t end;
	little_uint32_t fraction;
	little_uint32_t playCount;
} sampleLoop_t;

typedef struct SamplerChunk : waveChunkHead_t {
	little_uint32_t manufacturer;
	little_uint32_t product;
	little_uint32_t samplePeriod;
	little_uint32_t MIDIUnityNote;
	little_uint32_t MIDIPitchFraction;
	little_uint32_t SMPTEFormat;
	little_uint32_t SMPTEOffset;
	little_uint32_t sampleLoops;
	little_uint32_t samplerData;
	sampleLoop_t    loop[];
} smpl_t;

// Instrument Chunk 'inst'
typedef struct WaveInstrumentChunk : waveChunkHead_t {
	unsigned char unshiftedNote;
	char          fineTune;
	char          gain;
	unsigned char lowNote;
	unsigned char highNote;
	unsigned char lowVelocity;
	unsigned char highVelocity;
} inst_t;

//	Peak Chunk ??

#endif
