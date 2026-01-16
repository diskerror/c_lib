//
// Created by Reid Woodbury.
//

#ifndef DISKERROR_AUDIOFILE_H
#define DISKERROR_AUDIOFILE_H

#include <filesystem>
#include <fstream>
#include <vector>
#include <memory>
#include <span>
#include <string>
#include <map>

#include <boost/endian/arithmetic.hpp>

#include "AIFF.h"
#include "WAVE.h"
#include "DiskerrorExceptions.h"

namespace Diskerror {

using namespace std;
using namespace boost;
using namespace boost::endian;

//	Currently only WAVE and AIFF (AIFC) is supported.
//	RIFF or RF64 is determined by total file size.
typedef enum {
	BASE_TYPE_UNKNOWN = 0,
	BASE_TYPE_WAVE, //	RIFF or RF64: WAVE, little-endian (and BWF)
	BASE_TYPE_AIFF, //	FORM: AIFF or AIFC, big-endian
	BASE_TYPE_WAVX, //	RIFX: WAVE, big-endian (Not supported)
	BASE_TYPE_8SVX, //	FORM: 8SVX, Amiga IFF 8-bit, big-endian (Not supported)
	BASE_TYPE_MAUD  //	FORM: MAUD, Amiga IFF multi-channel, big-endian (Not supported)
} baseAudioFileType_t;

// Base class for all chunks
struct Chunk {
	fourcc_t id;
	virtual ~Chunk() = default;
	explicit Chunk(fourcc_t _id) : id(_id) {}

	// Serialize chunk data (excluding ID and Size header)
	virtual std::vector<uint8_t> serializePayload(bool isLittleEndian) const = 0;

	// Serialize full chunk (Header + Payload)
	// Handles padding byte if size is odd
	std::vector<uint8_t> serialize(bool isLittleEndian) const;
};

// Generic chunk for unknown or non-specialized chunks
struct UnknownChunk : public Chunk {
	std::vector<uint8_t> payload;

	UnknownChunk(fourcc_t _id, std::vector<uint8_t> _payload)
		: Chunk(_id), payload(std::move(_payload)) {}

	std::vector<uint8_t> serializePayload(bool) const override { return payload; }
};

// Specialized chunk for WAVE 'fmt '
struct WaveFmtChunk : public Chunk {
	uint16_t formatTag = 0;
	uint16_t numChannels = 0;
	uint32_t sampleRate = 0;
	uint32_t bytesPerSec = 0;
	uint16_t blockAlign = 0;
	uint16_t bitsPerSample = 0;
	std::vector<uint8_t> extension; // cbSize + extra bytes

	WaveFmtChunk() : Chunk(fourcc_t('fmt ')) {}
	std::vector<uint8_t> serializePayload(bool isLittleEndian) const override;
};

// Specialized chunk for AIFF 'COMM'
struct AiffCommChunk : public Chunk {
	uint16_t numChannels = 0;
	uint32_t numSampleFrames = 0;
	uint16_t sampleSize = 0;
	double   sampleRate = 0.0; // Stored as 80-bit float
	fourcc_t compressionType = 0;
	std::string compressionName; // Pascal string

	AiffCommChunk() : Chunk(fourcc_t('COMM')) {}
	std::vector<uint8_t> serializePayload(bool isLittleEndian) const override;
};

// Specialized chunk for Audio Data ('data' or 'SSND')
struct DataChunk : public Chunk {
	uint64_t fileOffset = 0; // Absolute position in file where payload starts
	uint32_t payloadSize = 0; // Size of audio data (excluding offset/blockSize in SSND)

	// For AIFF SSND
	uint32_t offset = 0;
	uint32_t blockSize = 0;

	// In-memory buffer for new/modified data before flush
	std::vector<uint8_t> memoryBuffer;

	explicit DataChunk(fourcc_t _id) : Chunk(_id) {}
	std::vector<uint8_t> serializePayload(bool isLittleEndian) const override;
};

struct AudioFormat {
	uint32_t sampleRate = 0;
	uint16_t numChannels = 0;
	uint16_t bitsPerSample = 0;
	uint32_t bytesPerFrame = 0;
	bool     isFloatingPoint = false;
	bool     isLittleEndian = false;
	fourcc_t encoding = 0;
};

struct ChunkInfo {
	fourcc_t id;
	uint32_t size;
	size_t   index;
};

class AudioFile {
public:
	// Builder for creating new files
	class Builder {
		filesystem::path m_path;
		baseAudioFileType_t m_type = BASE_TYPE_WAVE;
		uint32_t m_sampleRate = 44100;
		uint16_t m_numChannels = 2;
		uint16_t m_bitsPerSample = 16;
		bool m_isFloat = false;
	public:
		Builder& setPath(const filesystem::path& p) { m_path = p; return *this; }
		Builder& setType(baseAudioFileType_t t) { m_type = t; return *this; }
		Builder& setSampleRate(uint32_t r) { m_sampleRate = r; return *this; }
		Builder& setNumChannels(uint16_t c) { m_numChannels = c; return *this; }
		Builder& setBitsPerSample(uint16_t b) { m_bitsPerSample = b; return *this; }
		Builder& setIsFloat(bool f) { m_isFloat = f; return *this; }
		std::unique_ptr<AudioFile> build();
	};

	// Open existing file
	explicit AudioFile(const filesystem::path& fPath, ios_base::openmode mode = ios_base::in | ios_base::out);

	// Create new file (internal use by Builder)
	AudioFile(
		const filesystem::path& fPath,
		baseAudioFileType_t     type,
		uint32_t                sampleRate,
		uint16_t                numChannels,
		uint16_t                bitsPerSample,
		bool                    isFloat
	);

	~AudioFile();

	// Delete copy, allow move
	AudioFile(const AudioFile&) = delete;
	AudioFile& operator=(const AudioFile&) = delete;
	AudioFile(AudioFile&&) noexcept;
	AudioFile& operator=(AudioFile&&) noexcept;

	// File Info
	string              getFileName() const;
	baseAudioFileType_t getBaseType() const;
	const AudioFormat&  getFormat() const;
	int64_t             getNumFrames() const;

	// Stream-like I/O (constrained to audio data)
	void     read(void* buffer, size_t size);
	void     write(const void* buffer, size_t size);
	void     seekg(int64_t offset, ios_base::seekdir dir = ios_base::beg);
	void     seekp(int64_t offset, ios_base::seekdir dir = ios_base::beg);
	uint64_t tellg();
	uint64_t tellp();
	void     flush();

	// Chunk Management
	std::vector<ChunkInfo>      chunkList() const;
	size_t                      getChunkCount(fourcc_t id) const;
	std::vector<unsigned char>  getChunk(fourcc_t id, size_t index = 0);
	void                        addChunk(fourcc_t id, const std::vector<unsigned char>& payload);
	void                        addChunk(fourcc_t id, const void* data, size_t size);
	void                        deleteChunk(fourcc_t id, size_t index = 0);

	// Internal helpers for AudioFormat
	void updateFormat(uint32_t sampleRate, uint16_t numChannels, uint16_t bitsPerSample, bool isFloat);
	void updateAudioData(const std::vector<uint8_t>& pcmData);

private:
	const filesystem::path the_file_path;
	fstream                file_access;
	baseAudioFileType_t    base_type = BASE_TYPE_UNKNOWN;
	AudioFormat            format;

	std::vector<std::shared_ptr<Chunk>> m_chunks;
	std::shared_ptr<DataChunk>          m_dataChunk;
	std::shared_ptr<Chunk>              m_formatChunk; // Weak ref to fmt/COMM in m_chunks

	uint64_t m_readPos = 0;  // Relative to audio data start
	uint64_t m_writePos = 0; // Relative to audio data start

	void parseHeader();
	void parseWAVE();
	void parseAIFF();
	void syncFormatFromChunks();
	bool isReserved(fourcc_t id) const;
    uint32_t calculatePadding(uint64_t currentOffset, uint32_t alignment, uint32_t nextChunkHeaderSize) const;

	static constexpr uint32_t WAVE_ALIGNMENT = 4096;
	static constexpr uint32_t AIFF_ALIGNMENT = 512;
};

} // namespace Diskerror

#endif // DISKERROR_AUDIOFILE_H
