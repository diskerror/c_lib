//
// Created by Reid Woodbury.
//

#ifndef DISKERROR_AUDIOSAMPLES_H
#define DISKERROR_AUDIOSAMPLES_H


#include <boost/cstdfloat.hpp>
#include <boost/endian.hpp>

#include "AudioFile.h"
#include "VectorMath.h"

namespace Diskerror {


using namespace std;
using namespace boost;
using namespace boost::endian;

/**
 *	class AudioSamples
 */
class AudioSamples {
	AudioFile &audioFile;

	static void ReverseCopy4Bytes(unsigned char*, const unsigned char*);

	void assertDataFormat() const;

public:
	// Constructor
	explicit AudioSamples(AudioFile&); //	Needs pointer to Diskerror::AudioFile

	// Destructor
	~AudioSamples();


	// Exposing these members because of their useful methods.
	VectorMath<float32_t> samples;

	float32_t GetSampleMaxMagnitude() const;

	void ReadSamples();

	void Normalize();

	static float32_t Dither();

	void WriteSamples(bool do_dither = true); //	True == do dither on converstion.

	float32_t& operator[](uint64_t); //	Returns sample at index.
};


} // namespace Diskerror

#endif // DISKERROR_AUDIOSAMPLES_H
