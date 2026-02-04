//
// Created by Reid Woodbury.
//

#ifndef DISKERROR_AUDIOFORMAT_H
#define DISKERROR_AUDIOFORMAT_H

#include <cstdint>
#include <span>
#include <vector>

#include "AudioTypes.h"
#include "WAVE.h"
#include "AIFF.h"

namespace Diskerror {

class AudioFile;  // forward declaration

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
	SampleEncoding   m_encoding = SampleEncoding::PCM;
	AudioType        m_type     = AudioType::Unknown;
	const AudioFile* m_file     = nullptr;  // For calculating WAVE frame count from dataSize
	fmt_t            m_waveFmt{};
	COMM_t           m_aiffComm{};

public:
	AudioFormat() = default;

	//	Construct from an AudioFile: reads type, finds format chunk, parses it.
	//	If no format chunk exists (new file), initializes sensible defaults for the type.
	explicit AudioFormat(const AudioFile& file);

	// --- Factory methods (standalone parsing) ---

	//	Parse a complete 'fmt ' chunk blob: [ID 4][SIZE 4][payload 16+]
	static AudioFormat fromWaveFmt(std::span<const uint8_t> blob);

	//	Parse a complete 'COMM' chunk blob: [ID 4][SIZE 4][payload 18+]
	static AudioFormat fromAiffComm(std::span<const uint8_t> blob);

	// --- Serialization ---

	//	Produce the appropriate format chunk blob based on stored type.
	//	Returns 'fmt ' blob for Wave, 'COMM' blob for Aiff.
	[[nodiscard]] std::vector<uint8_t> toChunk() const;

	// --- Query methods ---

	[[nodiscard]] SampleEncoding encoding() const { return m_encoding; }
	[[nodiscard]] AudioType type() const { return m_type; }
	[[nodiscard]] bool isLinear() const { return m_encoding == SampleEncoding::PCM || m_encoding == SampleEncoding::Float; }
	[[nodiscard]] bool requiresAifc() const;

	// --- Convenience accessors (format-agnostic) ---
	//	These dispatch to the appropriate internal struct based on m_type.

	[[nodiscard]] uint16_t channels() const;
	[[nodiscard]] double   sampleRate() const;
	[[nodiscard]] uint16_t bitsPerSample() const;
	[[nodiscard]] uint32_t numSampleFrames() const;
	[[nodiscard]] uint16_t bytesPerFrame() const;
	[[nodiscard]] uint32_t bytesPerSecond() const;

	void setChannels(uint16_t c);
	void setSampleRate(double r);
	void setBitsPerSample(uint16_t b);
	void setNumSampleFrames(uint32_t n);

	//	Auto-compute frame count from audio data size (for linear formats).
	void updateFrameCount(int64_t dataSize);

	// --- Encoding setter ---

	void setEncoding(SampleEncoding e);

	// --- Direct struct access (for compressed/non-standard formats) ---

	[[nodiscard]] const fmt_t&  waveFmt() const  { return m_waveFmt; }
	[[nodiscard]] fmt_t&        waveFmt()        { return m_waveFmt; }
	[[nodiscard]] const COMM_t& aiffComm() const { return m_aiffComm; }
	[[nodiscard]] COMM_t&       aiffComm()       { return m_aiffComm; }

private:
	//	Internal serialization helpers
	[[nodiscard]] std::vector<uint8_t> toWaveFmt() const;
	[[nodiscard]] std::vector<uint8_t> toAiffComm() const;
};

} // namespace Diskerror

#endif // DISKERROR_AUDIOFORMAT_H
