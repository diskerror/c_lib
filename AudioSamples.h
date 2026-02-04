//
// Created by Reid Woodbury.
//

#ifndef DISKERROR_AUDIOSAMPLES_H
#define DISKERROR_AUDIOSAMPLES_H

#include <cstdint>
#include <vector>

#include <boost/cstdfloat.hpp>

#include "VectorMath.h"

namespace Diskerror {

using boost::float32_t;
using boost::float64_t;

/**
 * Deinterleaved audio buffer: buf[channel][sample]
 * Width (channels) and length (frames) are inherent to the vector dimensions.
 */
using AudioBuffer = std::vector<VectorMath<float32_t>>;

// Forward declarations
class AudioFile;
class AudioFormat;

/**
 * AudioSamples
 *
 * Reads and writes audio data through AudioFile, using AudioFormat to
 * interpret the byte layout. All samples are converted to/from normalized
 * float32 in the range [-1.0, 1.0].
 *
 * PCM data is normalized by dividing by 2^(bits-1).
 * Values outside [-1.0, 1.0] are clipped on write.
 * 8-bit WAVE (unsigned) and AIFF (signed) are handled correctly.
 *
 * Reading 64-bit float files throws a catchable exception (lossy conversion).
 */
class AudioSamples {
    AudioFile*         file_;
    const AudioFormat* format_;

    // Conversion helpers
    [[nodiscard]] double getNormalizationDivisor() const;
    [[nodiscard]] bool   is8BitUnsigned() const;

    // Internal read/write for a contiguous block of raw bytes
    void readRawFrames(uint64_t startFrame, uint64_t frameCount,
                       std::vector<uint8_t>& rawData) const;
    void writeRawFrames(uint64_t startFrame,
                        const std::vector<uint8_t>& rawData);

    // Conversion between raw bytes and normalized float buffer
    AudioBuffer rawToFloat(const std::vector<uint8_t>& raw,
                           uint64_t frameCount) const;
    std::vector<uint8_t> floatToRaw(const AudioBuffer& buf,
                                    bool dither) const;

public:
    AudioSamples(AudioFile* file, const AudioFormat* format);

    // Read deinterleaved float32 samples (normalized to [-1.0, 1.0])
    // Throws FormatError if 64-bit float source (lossy), catchable to continue.
    [[nodiscard]] AudioBuffer readAll();
    [[nodiscard]] AudioBuffer readRange(uint64_t startFrame, uint64_t frameCount);

    // Write from deinterleaved float32 samples.
    // Values outside [-1.0, 1.0] are clipped to target range.
    // Dither applied only to integer targets when enabled.
    void writeAll(const AudioBuffer& buf, bool dither = true);
    void writeRange(const AudioBuffer& buf, uint64_t startFrame, bool dither = true);

    // Triangular dither generator: random value in (-1, 1) with triangular PDF.
    [[nodiscard]] static float32_t dither();

    // Normalize buffer so peak magnitude equals target (default 1.0).
    // Finds the maximum absolute value across all channels and scales uniformly.
    static void normalize(AudioBuffer& buf, float32_t target = 1.0f);
};

} // namespace Diskerror

#endif // DISKERROR_AUDIOSAMPLES_H
