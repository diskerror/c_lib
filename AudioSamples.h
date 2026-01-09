//
// Created by Reid Woodbury.
//

#ifndef DISKERROR_AUDIOSAMPLES_H
#define DISKERROR_AUDIOSAMPLES_H

#include <filesystem>
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
class AudioSamples : public VectorMath<float32_t>, public AudioFile {
	void assertDataFormat() const;

public:
	// Constructor
	explicit AudioSamples(const filesystem::path&);

	// Destructor
	~AudioSamples() = default;


	[[nodiscard]] float64_t getSampleMaxMagnitude() const;

	void ReadSamples();

	void Normalize();

	static float32_t Dither();

	void WriteSamples(bool do_dither = true); //	True == do dither on converstion to smaller int.
};


} // namespace Diskerror

#endif // DISKERROR_AUDIOSAMPLES_H
