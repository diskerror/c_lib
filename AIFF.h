/**
 *	AIFF file format.
 */

#ifndef DISKERROR_AIFF_H
#define DISKERROR_AIFF_H
#pragma once

#include <boost/endian/arithmetic.hpp>

#include "BigFloat80.h"

using namespace boost::endian;

//	Four-character code.
typedef big_uint32_t fourcc_t;

typedef struct AiffFileHeader {
	fourcc_t     id   = 'FORM';
	big_uint32_t size = 0;      //	file total size - 8
	fourcc_t     type = 'AIFF'; //	AIFF or AIFC
} aiffFileHeader_t;

// Parent class
typedef struct AiffChunk {
	fourcc_t     id   = 0;
	big_uint32_t size = 0;
} aiffChunkHead_t;

typedef struct aChunk : aiffChunkHead_t {
//	fourcc_t	 id;
//  big_uint32_t size;
	char8_t data[]; //	data[size]
} aChunk_t;

//	Data format structure.
//	Chunk data with type 'COMM' and size 18 will have this structure.
typedef struct DataFormatChunk : aiffChunkHead_t {
	big_uint16_t numChannels     = 0; // 1 = mono, 2 = stereo, etc.
	big_uint32_t numSampleFrames = 0; // a frame is one sample for each channel
	big_uint16_t sampleSize      = 0; // valid bits per sample 8, 16, 24, etc.
	// sample rate: 32000, 44100, 48000, etc. as a big endian 80-bit extended float
	Diskerror::big_float80_t sampleRate = 0;
} commChunk_t;

//	Chunk data with type 'COMM' and size >= 23 will have this structure.
typedef struct ExtDataFormatChunk : DataFormatChunk {
// big_uint16_t numChannels;        // 1 = mono, 2 = stereo, etc.
// big_uint32_t numSampleFrames;    // a frame is one sample for each channel
// big_uint16_t sampleSize;         // valid bits per sample 8, 16, 24, etc.
// big_ext80_t  sampleRate;         // 32000, 44100, 48000, etc. as a big endian 80-bit extended float
	fourcc_t compressionType = 0;
	char     compressionName[256]; // variable length array, Pascal string
} commExtChunk_t, COMM_t;

//  Sound data chunk. 'SSND'
//	dataBlockSize should be the chunk size - 8 for offset and blockSize members
//  dataBlockSize will be derived from numSampleFrames * numChannels * ceil(sampleSize / 8)
typedef struct SoundDataChunk : aiffChunkHead_t {
	big_uint32_t offset    = 0; // Determines where the first sample frame in the soundData starts. Offset is in bytes.
	big_uint32_t blockSize = 0;
	char8_t      data[]; //	data[size+8]
} ssndChunk_t, SSND_t;

// offset  determines where the first sample frame in the soundData starts.  offset is in bytes.
//    Most applications won't use offset and should set it to zero.  Use for a non-zero offset
//    is explained in the Block-Aligning Sound Data section below.

// blockSize  is used in conjunction with offset for block-aligning sound data.
//    It contains the size in bytes of the blocks that sound data is aligned to.
//    As with offset, most applications won't use blockSize and should set it to zero.
//    More information on blockSize is in the Block-Aligning Sound Data section below.

// soundData  contains the sample frames that make up the sound.
//    The number of sample frames in the soundData is determined by the numSampleFrames parameter in the Common Chunk.

//	Marker Chunk Format 'MARK'
//	each marker
typedef big_int16_t markerID_t;

typedef struct MarkerEntryChunk {
	markerID_t   id       = 0;
	big_uint32_t position = 0;
	char         name[]; //	Pascal string, max 256 char
} marker_t;

typedef struct MarkerChunk : aiffChunkHead_t {
	big_uint16_t numMarkers = 0;
	marker_t     markers[];
} MARK_t;

//	Instrument Chunk 'INST'
//	each loop
typedef struct LoopChunk {
	big_int16_t playMode  = 0; //	0=No Looping, 1=Forward loop, 2=Forward and Backward
	markerID_t  beginLoop = 0;
	markerID_t  endLoop   = 0;
} loop_t;

typedef struct InstrumentChunk : aiffChunkHead_t {
	char        baseNote     = 0;
	char        detune       = 0;
	char        lowNote      = 0;
	char        highNote     = 0;
	char        lowVelocity  = 0;
	char        highVelocity = 127;
	big_int16_t gain         = 0;
	loop_t      sustainLoop  = {0, 0, 0};
	loop_t      releaseLoop  = {0, 0, 0};
} INST_t;

//	Generic data chunk, such as:
//		MIDI Data Chunk 'MIDI'
//		Audio Recording Chunk 'AESD' (data length is 24)
typedef struct GenericDataChunk : aiffChunkHead_t {
	unsigned char data[];
} aiffData_t;

//	Application Specific Chunk 'APPL'
typedef struct ApplicationChunk : aiffChunkHead_t {
	fourcc_t applicationSignature = 0;
	char     data[];
} APPL_t;

//	Comments Chunk
typedef struct CommentChunk {
	big_uint32_t timeStamp = 0;;
	markerID_t   marker    = 0;;
	big_uint16_t count     = 0;; //	length of text
	char         text[];
} comment_t;

typedef struct CommentsChunk : aiffChunkHead_t {
	big_uint16_t numComments = 0;;
	comment_t    comments[];
} COMT_t;

//	Text Chunks - Name 'NAME', Author 'AUTH', Copyright '(c) ', Annotation 'ANNO'
typedef struct TextChunk : aiffChunkHead_t {
	char text[]; //	ASCII, not a pstring
} text_t;

//	Sound Accelerator (Saxel) Chunk 'SAXL'
typedef struct SoundAcceleratorChunk {
	markerID_t   marker = 0;;
	big_uint16_t size   = 0;;
	char         data[];
} saxel_t;

typedef struct SaxelChunk : aiffChunkHead_t {
	big_uint16_t numSaxels = 0;;
	saxel_t      saxel[];
} SAXL_t;

#endif /* DISKERROR_AIFF_H */
