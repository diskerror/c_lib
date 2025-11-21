//
// Created by Reid Woodbury.
//

#include "AudioSamples.h"

#include <boost/cstdfloat.hpp>
#include <boost/endian.hpp>
#include <functional>
#include <iostream>
#include <random>
#include <stdexcept>

namespace Diskerror {

using namespace std;
using namespace boost;
using namespace boost::endian;


AudioSamples::AudioSamples(const char* aFile) : AudioFile(aFile) {}

void AudioSamples::assertDataFormat() const {
	if (!this->is_pcm() && !this->is_ieee()) {
		throw runtime_error(
			"ERROR: This method only handles PCM (integer) or IEEE (32-bit float) data in audio files.");
	}

	if (this->is_pcm() && this->getBitsPerSample() >= 32) {
		throw out_of_range(
			"ERROR: Cannot convert 32-bit integer (PCM) data to 32-bit float without loss of resolution.");
	}

	if (this->getBitsPerSample() > 32) {
		throw out_of_range("ERROR: Cannot read data without loss of resolution.");
	}
}



//	Maximum value storable == 2^(nBits-1) - 1
float64_t AudioSamples::getSampleMaxMagnitude() const {
	if (this->is_ieee())
		return 1.0;

	// return pow(2.0, this->audioFile->getBitsPerSample() - 1) - 1;

	switch (static_cast<int16_t>(ceil(this->getBitsPerSample() / 8.0))) {
		case 1:
			return 127.0;

		case 2:
			return 32767.0;

		case 3:
			return 8388607.0;

		case 4:
			return 2147483648.0;
	}

	throw runtime_error("Bad number of bits per sample	.");

	// return numeric_limits<float64_t>::quiet_NaN();
}


float32_t AudioSamples::Dither() {
	static std::random_device                        rd;
	static std::mt19937                              gen(rd());
	static std::uniform_real_distribution<float32_t> low(-1, 0);
	static std::uniform_real_distribution<float32_t> hi(0, 1);

	return low(gen) + hi(gen);
}


////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * ReadSamples
 * This method converts 8-, 16-, 24-bit integers (PCM), and returns 32-bit machine native floats (IEEE).
 */
void AudioSamples::ReadSamples() {
	this->assertDataFormat();

	if (this->getBitsPerSample() > 32 ||
		(this->is_pcm() && this->getBitsPerSample() >= 32)
	) {
		throw runtime_error("ERROR: Cannot read data without loss of resolution.");
	}

	auto dataBlock       = this->ReadAllData();
	auto tempLittleFloat = reinterpret_cast<little_float32_t*>(dataBlock);
	auto tempBigFloat    = reinterpret_cast<big_float32_t*>(dataBlock);

	uint64_t      numSamples = this->getNumFrames() * this->getNumChannels();
	uint_fast64_t si         = 0; //	Index variable for samples.

	this->resize(numSamples);
	this->shrink_to_fit();

	if (this->getBitsPerSample() == 8) {
		for (; si < numSamples; ++si) {
			(*this)[si] = static_cast<float32_t>(dataBlock[si] - 127);
		}
	}
	else {
		unsigned char* dPtr = dataBlock;

		if (this->is_littleEndian()) {
			switch (this->getBitsPerSample()) {
				case 16:
					for (auto& elem : *this) {
						elem = static_cast<float32_t>(load_little_s16(dPtr));
						dPtr += 2;
					}
					break;

				case 24:
					for (auto& elem : *this) {
						elem = static_cast<float32_t>(load_little_s24(dPtr));
						dPtr += 3;
					}
					break;

				//  Data is 32-bit float.
				case 32:
					for (; si < numSamples; ++si) {
						(*this)[si] = static_cast<float32_t>(tempLittleFloat[si]);
					}
					break;

				default:
					throw runtime_error("Should not get here.");
			}
		}
		//	is big endian
		else {
			switch (this->getBitsPerSample()) {
				case 16:
					for (auto& elem : *this) {
						elem = static_cast<float32_t>(load_big_s16(dPtr));
						dPtr += 2;
					}
					break;

				case 24:
					for (auto& elem : *this) {
						elem = static_cast<float32_t>(load_big_s24(dPtr));
						dPtr += 3;
					}
					break;

				//  Data is 32-bit float.
				case 32:
					for (; si < numSamples; ++si) {
						(*this)[si] = static_cast<float32_t>(tempBigFloat[si]);
					}
					break;

				default:
					throw runtime_error("Should not get here.");
			}
		}
	}

	delete dataBlock;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
void AudioSamples::Normalize() { this->normalize_mag(getSampleMaxMagnitude()); }


////////////////////////////////////////////////////////////////////////////////////////////////////
//	Assumes data will be the same size as the buffer that was read.
void AudioSamples::WriteSamples(const bool do_dither) {
	this->assertDataFormat();

	auto dataBlock       = static_cast<unsigned char*>(calloc(this->getDataSize(), 1));
	auto tempLittleFloat = reinterpret_cast<little_float32_t*>(dataBlock);
	auto tempBigFloat    = reinterpret_cast<big_float32_t*>(dataBlock);

	uint64_t      numSamples = this->getNumFrames() * this->getNumChannels();
	uint_fast64_t si         = 0; //	Index variable for samples.

	//	Decide whether to add dither.
	std::function getDither = do_dither ?
								  []()-> float32_t { return Dither(); } :
								  []()-> float32_t { return 0.0; };

	if (this->getBitsPerSample() == 8) {
		for (; si < numSamples; ++si) {
			dataBlock[si] = static_cast<uint8_t>((*this)[si] + getDither()) + 127;
		}
	}
	else {
		unsigned char* dPtr = dataBlock;

		if (this->is_littleEndian()) {
			switch (this->getBitsPerSample()) {
				case 16:
					for (auto& elem : *this) {
						store_little_s16(dPtr, static_cast<int16_t>(elem + getDither()));
						dPtr += 2;
					}
					break;

				case 24:
					for (auto& elem : *this) {
						store_little_s24(dPtr, static_cast<int32_t>(elem + getDither()));
						dPtr += 3;
					}
					break;

				case 32:
					for (; si < numSamples; ++si) {
						tempLittleFloat[si] = static_cast<little_float32_t>((*this)[si]); //	No dither used with floating point data.
					}
					break;

				default:
					throw runtime_error("Should not get here.");
			}
		}
		//	is big endian
		else {
			switch (this->getBitsPerSample()) {
				case 16:
					for (auto& elem : *this) {
						store_big_s16(dPtr, static_cast<int16_t>(elem + getDither()));
						dPtr += 2;
					}
					break;

				case 24:
					for (auto& elem : *this) {
						store_big_s24(dPtr, static_cast<int32_t>(elem + getDither()));
						dPtr += 3;
					}
					break;

				case 32:
					for (; si < numSamples; ++si) {
						tempBigFloat[si] = static_cast<big_float32_t>((*this)[si]); //	No dither used with floating point data.
					}
					break;

				default:
					throw runtime_error("Should not get here.");
			}
		}
	}

	this->WriteAllData(dataBlock);
	delete dataBlock;
}

} //  namespace Diskerror
