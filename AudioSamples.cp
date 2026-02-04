//
// Created by Reid Woodbury.
//

#include "AudioSamples.h"
#include "AudioFile.h"
#include "AudioFormat.h"
#include "DiskerrorExceptions.h"

#include <boost/endian/arithmetic.hpp>
#include <boost/endian/conversion.hpp>

#include <algorithm>
#include <cmath>
#include <random>

namespace Diskerror {

using namespace boost::endian;

// =============================================================================
// Construction
// =============================================================================

AudioSamples::AudioSamples(AudioFile* file, const AudioFormat* format)
    : file_(file), format_(format)
{
    if (!file_ || !format_) {
        throw UsageError("AudioSamples requires non-null AudioFile and AudioFormat pointers.");
    }
}

// =============================================================================
// Public API
// =============================================================================

AudioBuffer AudioSamples::readAll() {
    return readRange(0, format_->numSampleFrames());
}

AudioBuffer AudioSamples::readRange(uint64_t startFrame, uint64_t frameCount) {
    // Validate format
    const auto encoding = format_->encoding();
    const auto bits = format_->bitsPerSample();

    if (encoding != SampleEncoding::PCM && encoding != SampleEncoding::Float) {
        throw FormatError("AudioSamples only handles PCM or Float encoding.");
    }

    if (encoding == SampleEncoding::Float && bits == 64) {
        throw FormatError("Reading 64-bit float: lossy conversion to 32-bit float.");
    }

    if (encoding == SampleEncoding::PCM && bits > 32) {
        throw FormatError("PCM bit depth > 32 not supported.");
    }

    // Read raw bytes
    std::vector<uint8_t> rawData;
    readRawFrames(startFrame, frameCount, rawData);

    // Convert to normalized float buffer
    return rawToFloat(rawData, frameCount);
}

void AudioSamples::writeAll(const AudioBuffer& buf, bool dither) {
    writeRange(buf, 0, dither);
}

void AudioSamples::writeRange(const AudioBuffer& buf, uint64_t startFrame, bool dither) {
    if (buf.empty()) return;

    const auto channels = format_->channels();
    if (buf.size() != channels) {
        throw UsageError("AudioBuffer channel count does not match format.");
    }

    // Validate all channels have same length
    const auto frameCount = buf[0].size();
    for (size_t ch = 1; ch < channels; ++ch) {
        if (buf[ch].size() != frameCount) {
            throw UsageError("AudioBuffer channels have inconsistent frame counts.");
        }
    }

    // Convert to raw bytes
    std::vector<uint8_t> rawData = floatToRaw(buf, dither);

    // Write raw bytes
    writeRawFrames(startFrame, rawData);
}

float32_t AudioSamples::dither() {
    // Thread-local for thread safety
    thread_local std::random_device rd;
    thread_local std::mt19937 gen(rd());
    thread_local std::uniform_real_distribution<float32_t> dist(-0.5f, 0.5f);

    // Triangular PDF: sum of two uniform distributions
    return dist(gen) + dist(gen);
}

void AudioSamples::normalize(AudioBuffer& buf, float32_t target) {
    if (buf.empty()) return;

    // Find maximum magnitude across all channels
    float32_t maxMag = 0.0f;
    for (const auto& channel : buf) {
        float32_t chMax = channel.max_mag();
        if (chMax > maxMag) {
            maxMag = chMax;
        }
    }

    // Scale all channels uniformly
    if (maxMag > 0.0f) {
        float32_t scale = target / maxMag;
        for (auto& channel : buf) {
            channel *= scale;
        }
    }
}

// =============================================================================
// Internal Helpers
// =============================================================================

double AudioSamples::getNormalizationDivisor() const {
    const auto encoding = format_->encoding();
    const auto bits = format_->bitsPerSample();

    if (encoding == SampleEncoding::Float) {
        return 1.0;  // Float is already normalized
    }

    // PCM: 2^(bits-1)
    switch (bits) {
        case 8:  return 128.0;
        case 16: return 32768.0;
        case 24: return 8388608.0;
        case 32: return 2147483648.0;
        default:
            throw FormatError("Unsupported PCM bit depth: " + std::to_string(bits));
    }
}

bool AudioSamples::is8BitUnsigned() const {
    // WAVE 8-bit PCM is unsigned (0-255), AIFF 8-bit is signed (-128 to 127)
    return format_->bitsPerSample() == 8 &&
           format_->encoding() == SampleEncoding::PCM &&
           file_->type() == AudioType::Wave;
}

void AudioSamples::readRawFrames(uint64_t startFrame, uint64_t frameCount,
                                  std::vector<uint8_t>& rawData) const {
    const auto bytesPerFrame = format_->bytesPerFrame();
    const auto byteOffset = static_cast<std::streamoff>(startFrame * bytesPerFrame);
    const auto byteCount = frameCount * bytesPerFrame;

    rawData.resize(byteCount);

    // Seek and read - need non-const file for I/O
    auto* mutableFile = const_cast<AudioFile*>(file_);
    mutableFile->seekg(byteOffset);
    mutableFile->read(reinterpret_cast<char*>(rawData.data()),
                      static_cast<std::streamsize>(byteCount));
}

void AudioSamples::writeRawFrames(uint64_t startFrame,
                                   const std::vector<uint8_t>& rawData) {
    const auto bytesPerFrame = format_->bytesPerFrame();
    const auto byteOffset = static_cast<std::streamoff>(startFrame * bytesPerFrame);

    file_->seekp(byteOffset);
    file_->write(reinterpret_cast<const char*>(rawData.data()),
                 static_cast<std::streamsize>(rawData.size()));
}

// =============================================================================
// Raw <-> Float Conversion
// =============================================================================

AudioBuffer AudioSamples::rawToFloat(const std::vector<uint8_t>& raw,
                                      uint64_t frameCount) const {
    const auto channels = format_->channels();
    const auto bits = format_->bitsPerSample();
    const auto encoding = format_->encoding();
    const bool isLittle = file_->isLittleEndian();
    const double divisor = getNormalizationDivisor();
    const bool unsigned8 = is8BitUnsigned();

    // Allocate deinterleaved buffer
    AudioBuffer buf(channels);
    for (auto& ch : buf) {
        ch.resize(frameCount);
    }

    const uint8_t* ptr = raw.data();

    // Process frame by frame, deinterleaving
    for (uint64_t frame = 0; frame < frameCount; ++frame) {
        for (uint16_t ch = 0; ch < channels; ++ch) {
            double sample = 0.0;

            if (encoding == SampleEncoding::Float) {
                if (bits == 32) {
                    if (isLittle) {
                        auto val = *reinterpret_cast<const little_float32_t*>(ptr);
                        sample = static_cast<double>(val);
                    } else {
                        auto val = *reinterpret_cast<const big_float32_t*>(ptr);
                        sample = static_cast<double>(val);
                    }
                    ptr += 4;
                } else if (bits == 64) {
                    // Should have thrown earlier, but handle gracefully
                    if (isLittle) {
                        auto val = *reinterpret_cast<const little_float64_t*>(ptr);
                        sample = static_cast<double>(val);
                    } else {
                        auto val = *reinterpret_cast<const big_float64_t*>(ptr);
                        sample = static_cast<double>(val);
                    }
                    ptr += 8;
                }
            } else {
                // PCM
                switch (bits) {
                    case 8:
                        if (unsigned8) {
                            // WAVE: unsigned, center at 128
                            sample = (static_cast<double>(*ptr) - 128.0) / divisor;
                        } else {
                            // AIFF: signed
                            sample = static_cast<double>(static_cast<int8_t>(*ptr)) / divisor;
                        }
                        ptr += 1;
                        break;

                    case 16:
                        if (isLittle) {
                            auto val = *reinterpret_cast<const little_int16_t*>(ptr);
                            sample = static_cast<double>(val) / divisor;
                        } else {
                            auto val = *reinterpret_cast<const big_int16_t*>(ptr);
                            sample = static_cast<double>(val) / divisor;
                        }
                        ptr += 2;
                        break;

                    case 24:
                        if (isLittle) {
                            int32_t val = load_little_s24(ptr);
                            sample = static_cast<double>(val) / divisor;
                        } else {
                            int32_t val = load_big_s24(ptr);
                            sample = static_cast<double>(val) / divisor;
                        }
                        ptr += 3;
                        break;

                    case 32:
                        if (isLittle) {
                            auto val = *reinterpret_cast<const little_int32_t*>(ptr);
                            sample = static_cast<double>(val) / divisor;
                        } else {
                            auto val = *reinterpret_cast<const big_int32_t*>(ptr);
                            sample = static_cast<double>(val) / divisor;
                        }
                        ptr += 4;
                        break;

                    default:
                        throw FormatError("Unsupported bit depth in rawToFloat.");
                }
            }

            buf[ch][frame] = static_cast<float32_t>(sample);
        }
    }

    return buf;
}

std::vector<uint8_t> AudioSamples::floatToRaw(const AudioBuffer& buf,
                                               bool doDither) const {
    const auto channels = format_->channels();
    const auto bits = format_->bitsPerSample();
    const auto encoding = format_->encoding();
    const bool isLittle = file_->isLittleEndian();
    const double multiplier = getNormalizationDivisor();
    const bool unsigned8 = is8BitUnsigned();
    const auto frameCount = buf[0].size();

    // Calculate output size
    const auto bytesPerFrame = format_->bytesPerFrame();
    std::vector<uint8_t> raw(frameCount * bytesPerFrame);
    uint8_t* ptr = raw.data();

    // Only apply dither to integer formats
    const bool applyDither = doDither && (encoding == SampleEncoding::PCM);

    // Process frame by frame, interleaving
    for (size_t frame = 0; frame < frameCount; ++frame) {
        for (uint16_t ch = 0; ch < channels; ++ch) {
            double sample = static_cast<double>(buf[ch][frame]);

            if (encoding == SampleEncoding::Float) {
                // Clip to [-1.0, 1.0] for consistency
                sample = std::clamp(sample, -1.0, 1.0);

                if (bits == 32) {
                    auto val = static_cast<float32_t>(sample);
                    if (isLittle) {
                        *reinterpret_cast<little_float32_t*>(ptr) = val;
                    } else {
                        *reinterpret_cast<big_float32_t*>(ptr) = val;
                    }
                    ptr += 4;
                } else if (bits == 64) {
                    if (isLittle) {
                        *reinterpret_cast<little_float64_t*>(ptr) = sample;
                    } else {
                        *reinterpret_cast<big_float64_t*>(ptr) = sample;
                    }
                    ptr += 8;
                }
            } else {
                // PCM: scale, dither, clip, round
                double scaled = sample * multiplier;

                if (applyDither) {
                    scaled += static_cast<double>(dither());
                }

                switch (bits) {
                    case 8: {
                        // Clip to [-128, 127] range before offset
                        scaled = std::clamp(scaled, -128.0, 127.0);
                        int32_t rounded = static_cast<int32_t>(std::round(scaled));

                        if (unsigned8) {
                            // WAVE: add 128 offset for unsigned
                            *ptr = static_cast<uint8_t>(rounded + 128);
                        } else {
                            // AIFF: signed
                            *ptr = static_cast<uint8_t>(static_cast<int8_t>(rounded));
                        }
                        ptr += 1;
                        break;
                    }

                    case 16: {
                        scaled = std::clamp(scaled, -32768.0, 32767.0);
                        auto rounded = static_cast<int16_t>(std::round(scaled));

                        if (isLittle) {
                            *reinterpret_cast<little_int16_t*>(ptr) = rounded;
                        } else {
                            *reinterpret_cast<big_int16_t*>(ptr) = rounded;
                        }
                        ptr += 2;
                        break;
                    }

                    case 24: {
                        scaled = std::clamp(scaled, -8388608.0, 8388607.0);
                        auto rounded = static_cast<int32_t>(std::round(scaled));

                        if (isLittle) {
                            store_little_s24(ptr, rounded);
                        } else {
                            store_big_s24(ptr, rounded);
                        }
                        ptr += 3;
                        break;
                    }

                    case 32: {
                        scaled = std::clamp(scaled, -2147483648.0, 2147483647.0);
                        auto rounded = static_cast<int32_t>(std::round(scaled));

                        if (isLittle) {
                            *reinterpret_cast<little_int32_t*>(ptr) = rounded;
                        } else {
                            *reinterpret_cast<big_int32_t*>(ptr) = rounded;
                        }
                        ptr += 4;
                        break;
                    }

                    default:
                        throw FormatError("Unsupported bit depth in floatToRaw.");
                }
            }
        }
    }

    return raw;
}

} // namespace Diskerror