//
// Created by Reid Woodbury.
//

#ifndef DISKERROR_AUDIOFORMAT_H
#define DISKERROR_AUDIOFORMAT_H

#include <cstdint>
#include <span>
#include <vector>

#include <boost/endian/arithmetic.hpp>

namespace Diskerror {

using namespace boost::endian;

typedef big_uint32_t fourcc_t;

enum class SampleEncoding : uint8_t {
	Unknown = 0,
	PCM,        // WAVE 0x0001 / AIFC 'NONE','twos','sowt' / plain AIFF
	Float,      // WAVE 0x0003 / AIFC 'fl32','fl64'
	ULaw,       // WAVE 0x0007 / AIFC 'ulaw'
	ALaw,       // WAVE 0x0006 / AIFC 'ALAW'
	IMA_ADPCM,  // WAVE 0x0011 / AIFC 'ima4'
	Extensible, // WAVE 0xFFFE (real encoding in SubFormat GUID)
	Other,      // Valid but not natively supported
};

class AudioFormat {
	SampleEncoding m_encoding           = SampleEncoding::PCM;
	uint16_t       m_channels           = 1;
	double         m_sampleRate         = 44100.0;
	uint16_t       m_bitsPerSample      = 16;
	uint32_t       m_numSampleFrames    = 0;
	bool           m_sampleLittleEndian = false;
	uint16_t       m_rawWaveFormatTag   = 0;
	fourcc_t       m_rawAifcCompression{0};

public:
	AudioFormat() = default;

	// --- Factory methods ---

	//	Parse a complete 'fmt ' chunk blob: [ID 4][SIZE 4][payload 16+]
	static AudioFormat fromWaveFmt(std::span<const uint8_t> blob);

	//	Parse a complete 'COMM' chunk blob: [ID 4][SIZE 4][payload 18+]
	static AudioFormat fromAiffComm(std::span<const uint8_t> blob);

	// --- Serialization ---

	//	Produce a complete 'fmt ' chunk blob: [ID 4][SIZE 4][payload]
	[[nodiscard]] std::vector<uint8_t> toWaveFmt() const;

	//	Produce a complete 'COMM' chunk blob: [ID 4][SIZE 4][payload]
	[[nodiscard]] std::vector<uint8_t> toAiffComm() const;

	// --- Query methods ---

	[[nodiscard]] bool isLinear() const { return m_encoding == SampleEncoding::PCM || m_encoding == SampleEncoding::Float; }
	[[nodiscard]] bool requiresAifc() const { return !(m_encoding == SampleEncoding::PCM && !m_sampleLittleEndian); }

	[[nodiscard]] SampleEncoding encoding() const { return m_encoding; }
	[[nodiscard]] uint16_t channels() const { return m_channels; }
	[[nodiscard]] double sampleRate() const { return m_sampleRate; }
	[[nodiscard]] uint16_t bitsPerSample() const { return m_bitsPerSample; }
	[[nodiscard]] uint32_t numSampleFrames() const { return m_numSampleFrames; }
	[[nodiscard]] bool sampleLittleEndian() const { return m_sampleLittleEndian; }
	[[nodiscard]] uint16_t rawWaveFormatTag() const { return m_rawWaveFormatTag; }
	[[nodiscard]] fourcc_t rawAifcCompression() const { return m_rawAifcCompression; }

	[[nodiscard]] uint16_t bytesPerFrame() const {
		return m_channels * ((m_bitsPerSample + 7) / 8);
	}

	[[nodiscard]] uint32_t bytesPerSecond() const {
		return bytesPerFrame() * static_cast<uint32_t>(m_sampleRate);
	}

	void updateFrameCount(int64_t dataSize) {
		if (isLinear() && bytesPerFrame() > 0)
			m_numSampleFrames = static_cast<uint32_t>(dataSize / bytesPerFrame());
	}

	// --- Setters ---

	void setEncoding(SampleEncoding e) { m_encoding = e; }
	void setChannels(uint16_t c) { m_channels = c; }
	void setSampleRate(double r) { m_sampleRate = r; }
	void setBitsPerSample(uint16_t b) { m_bitsPerSample = b; }
	void setNumSampleFrames(uint32_t n) { m_numSampleFrames = n; }
	void setSampleLittleEndian(bool le) { m_sampleLittleEndian = le; }
};

} // namespace Diskerror

#endif // DISKERROR_AUDIOFORMAT_H
