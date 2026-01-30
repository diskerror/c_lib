//
// Created by Reid Woodbury.
//

#ifndef DISKERROR_AUDIOFILE_H
#define DISKERROR_AUDIOFILE_H

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <span>
#include <vector>

#include <boost/endian/arithmetic.hpp>

#include "AudioFormat.h"

namespace Diskerror {

using namespace boost::endian;

typedef big_uint32_t fourcc_t;

enum class AudioType : uint8_t {
	Unknown = 0,
	Wave,
	Aiff
};

/**
 *	AudioFile
 *
 *	Maps std::fstream to the needs of a WAVE or AIFF audio file.
 *	Only understands two chunk types: the 12-byte container header
 *	(RIFF/FORM/RF64) and the audio data chunk ('data'/'SSND').
 *	All other chunks are opaque byte blobs — the user's full responsibility.
 *
 *	Read and write positions are relative to the first byte of audio data.
 *	AudioFile tracks these positions internally so that flush() can rewrite
 *	chunk headers without disrupting the user's audio I/O position.
 */
class AudioFile {
	std::filesystem::path m_path;
	AudioType             m_type = AudioType::Unknown;
	std::fstream          m_file;

	//	12-byte container header: [ID 4][SIZE 4][TYPE 4]
	struct {
		fourcc_t id{0};
		union {
			little_uint32_t lSize;
			big_uint32_t    bSize;
		};
		fourcc_t type{0};
	} m_header{};

	//	Parsed format information (from 'fmt ' or 'COMM' chunk).
	AudioFormat m_format;

	//	Non-audio chunks stored as opaque byte blobs.
	//	Each vector is a complete chunk: [ID 4][SIZE 4][payload...]
	//	The data/SSND chunk and format chunk are NOT in this list.
	std::vector<std::vector<uint8_t>> m_chunks;

	//	Audio data region tracking
	int64_t m_dataStart = 0;    //	file offset of first audio byte
	int64_t m_dataSize  = 0;    //	current audio payload size in bytes
	int64_t m_readPos   = 0;    //	read position relative to m_dataStart
	int64_t m_writePos  = 0;    //	write position relative to m_dataStart
	bool    m_dirty     = false;

	//	Internal helpers
	int64_t allChunksSize() const;
	void    writeHeaders();

public:
	//	Open existing file. Throws if file does not exist.
	explicit AudioFile(const std::filesystem::path& path);

	//	Create new file. Throws if file already exists.
	//	User must add chunks (format, etc.) then call flush() before writing audio.
	AudioFile(const std::filesystem::path& path, AudioType type);

	//	Move semantics
	AudioFile(AudioFile&& other) noexcept;
	AudioFile& operator=(AudioFile&& other) noexcept;

	//	No copy
	AudioFile(const AudioFile&) = delete;
	AudioFile& operator=(const AudioFile&) = delete;

	//	Destructor — flushes if dirty
	~AudioFile();

	// --- Chunk management ---

	//	Add a fully-formed chunk. AudioFile copies the bytes.
	//	Returns the index of the inserted chunk.
	size_t addChunk(const void* data, size_t totalSize);

	//	Number of non-audio chunks.
	[[nodiscard]] size_t chunkCount() const;

	//	View of stored chunk bytes at the given index.
	[[nodiscard]] std::span<const uint8_t> chunk(size_t index) const;

	//	Replace the chunk at index with new data.
	void replaceChunk(size_t index, const void* data, size_t totalSize);

	//	Remove the chunk at index.
	void deleteChunk(size_t index);

	// --- Audio data I/O (positions relative to start of audio data) ---

	std::streamsize read(char* buf, std::streamsize count);
	std::streamsize write(const char* buf, std::streamsize count);

	void seekg(std::streamoff off, std::ios_base::seekdir dir = std::ios_base::beg);
	void seekp(std::streamoff off, std::ios_base::seekdir dir = std::ios_base::beg);

	[[nodiscard]] std::streampos tellg() const;
	[[nodiscard]] std::streampos tellp() const;

	// --- File operations ---

	//	Sync headers to disk. On a new file, this creates the file and
	//	establishes the header structure. Must be called before write().
	void flush();

	// --- Queries ---

	[[nodiscard]] bool isLittleEndian() const;
	[[nodiscard]] AudioType type() const;
	[[nodiscard]] int64_t dataSize() const;
	[[nodiscard]] const std::filesystem::path& path() const;
	[[nodiscard]] const AudioFormat& format() const;
	AudioFormat& format();
};

} // namespace Diskerror

#endif // DISKERROR_AUDIOFILE_H
