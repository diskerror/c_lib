/**
	WAVE file format.
	https://ccrma.stanford.edu/courses/422/projects/WaveFormat/
*/

#ifndef WAVE_H
#define WAVE_H
#pragma once

#include "types.h"

struct Chunk {
	char chunkID[4];
	ulong chunkSize;
};

struct HeaderChunk : Chunk {	//	'RIFF', int
	char format[4];				//	'WAVE'
};

struct FormatChunk : Chunk {	//	'fmt ', int
	ushort audioFormat;			//	PCM = 1 (i.e. Linear quantization)
	ushort numChannels;			//	Mono = 1, Stereo = 2, etc.
	ulong sampleRate;			//	8000, 44100, etc.
	ulong byteRate;				//	== SampleRate * NumChannels * BitsPerSample/8
	ushort blockAlign;			//	== NumChannels * BitsPerSample/8
	ushort bitsPerSample;		//	8 bits = 8, 16 bits = 16, etc.
};

struct DataChunk : Chunk {
	char data[0];

#endif /* WAVE_H */
