//
// Created by Reid Woodbury.
//

#include "AudioFile.h"
#include "DiskerrorExceptions.h"

#include <algorithm>
#include <cstring>

namespace Diskerror {

using namespace std;
using namespace boost::endian;


////////////////////////////////////////////////////////////////////////////////
//	Internal helpers
////////////////////////////////////////////////////////////////////////////////

//	Total on-disk size of all stored non-audio chunks, including word-alignment pad bytes.
int64_t AudioFile::allChunksSize() const {
	int64_t total = 0;
	for (auto& c : m_chunks) {
		//	Chunk on disk = ID(4) + SIZE(4) + payload(SIZE bytes).
		//	The vector already stores [ID][SIZE][payload], so c.size() is the total.
		total += static_cast<int64_t>(c.size());
		//	IFF word alignment: odd-sized payloads get a pad byte.
		//	Payload size is stored at bytes 4-7.
		uint32_t payloadSize;
		if (m_type == AudioType::Wave) {
			little_uint32_t ls;
			memcpy(&ls, c.data() + 4, 4);
			payloadSize = ls;
		}
		else {
			big_uint32_t bs;
			memcpy(&bs, c.data() + 4, 4);
			payloadSize = bs;
		}
		if (payloadSize & 1)
			total += 1;
	}
	return total;
}


//	Write container header, all non-audio chunks (in spec order), alignment chunk, and data/SSND header.
//	Does NOT touch the audio payload.
void AudioFile::writeHeaders() {
	const int64_t dataHdrSize = (m_type == AudioType::Wave) ? 8 : 16;

	//	For AIFF containers, structurally detect AIFC from COMM chunk payload size.
	if (m_type == AudioType::Aiff) {
		auto commIdx = findChunk('COMM');
		bool isAifc = false;
		if (commIdx.has_value()) {
			auto& commBlob = m_chunks[*commIdx];
			if (commBlob.size() > 8) {
				big_uint32_t bs;
				memcpy(&bs, commBlob.data() + 4, 4);
				isAifc = (static_cast<uint32_t>(bs) > 18);
			}
		}
		m_header.type = isAifc ? 'AIFC' : 'AIFF';
	}

	//	Update container header size field
	//	Total file size = m_dataStart + m_dataSize
	//	Container SIZE = total file size - 8
	if (m_type == AudioType::Wave)
		m_header.lSize = static_cast<uint32_t>(m_dataStart + m_dataSize - 8);
	else
		m_header.bSize = static_cast<uint32_t>(m_dataStart + m_dataSize - 8);

	m_file.seekp(0, ios_base::beg);

	//	Write 12-byte container header
	m_file.write(reinterpret_cast<const char*>(&m_header), 12);

	//	Build ordered chunk index: ds64, FVER, fmt/COMM, fact, then everything else.
	vector<size_t> order;
	order.reserve(m_chunks.size());
	for (size_t i = 0; i < m_chunks.size(); ++i) {
		fourcc_t cid;
		memcpy(&cid, m_chunks[i].data(), 4);
		if (cid == 'ds64') order.push_back(i);
	}
	for (size_t i = 0; i < m_chunks.size(); ++i) {
		fourcc_t cid;
		memcpy(&cid, m_chunks[i].data(), 4);
		if (cid == 'FVER') order.push_back(i);
	}
	for (size_t i = 0; i < m_chunks.size(); ++i) {
		fourcc_t cid;
		memcpy(&cid, m_chunks[i].data(), 4);
		if (cid == 'fmt ' || cid == 'COMM') order.push_back(i);
	}
	for (size_t i = 0; i < m_chunks.size(); ++i) {
		fourcc_t cid;
		memcpy(&cid, m_chunks[i].data(), 4);
		if (cid == 'fact') order.push_back(i);
	}
	for (size_t i = 0; i < m_chunks.size(); ++i) {
		fourcc_t cid;
		memcpy(&cid, m_chunks[i].data(), 4);
		if (cid != 'ds64' && cid != 'FVER' && cid != 'fmt ' && cid != 'COMM' && cid != 'fact')
			order.push_back(i);
	}

	//	Write all non-audio chunks in order
	for (auto i : order) {
		auto& c = m_chunks[i];
		m_file.write(reinterpret_cast<const char*>(c.data()), static_cast<streamsize>(c.size()));
		//	Word-alignment pad byte
		uint32_t payloadSize;
		if (m_type == AudioType::Wave) {
			little_uint32_t ls;
			memcpy(&ls, c.data() + 4, 4);
			payloadSize = ls;
		}
		else {
			big_uint32_t bs;
			memcpy(&bs, c.data() + 4, 4);
			payloadSize = bs;
		}
		if (payloadSize & 1) {
			const char zero = 0;
			m_file.write(&zero, 1);
		}
	}

	//	Calculate fill space for alignment chunk
	//	fill = m_dataStart - 12 - allChunksSize() - 8 (alignment hdr) - dataHdrSize
	int64_t fillPayload = m_dataStart - 12 - allChunksSize() - 8 - dataHdrSize;
	if (fillPayload < 0)
		throw FormatError("Chunks exceed allocated header space.");

	//	Write alignment chunk (JUNK for WAVE, PAD for AIFF)
	if (m_type == AudioType::Wave) {
		fourcc_t        junkId   = 'JUNK';
		little_uint32_t junkSize = static_cast<uint32_t>(fillPayload);
		m_file.write(reinterpret_cast<const char*>(&junkId), 4);
		m_file.write(reinterpret_cast<const char*>(&junkSize), 4);
	}
	else {
		fourcc_t     padId   = 'PAD ';
		big_uint32_t padSize = static_cast<uint32_t>(fillPayload);
		m_file.write(reinterpret_cast<const char*>(&padId), 4);
		m_file.write(reinterpret_cast<const char*>(&padSize), 4);
	}
	//	Write fill bytes (zeros)
	if (fillPayload > 0) {
		vector<char> zeros(fillPayload, 0);
		m_file.write(zeros.data(), fillPayload);
	}

	//	Write data chunk header
	if (m_type == AudioType::Wave) {
		fourcc_t        dataId   = 'data';
		little_uint32_t dataSize = static_cast<uint32_t>(m_dataSize);
		m_file.write(reinterpret_cast<const char*>(&dataId), 4);
		m_file.write(reinterpret_cast<const char*>(&dataSize), 4);
	}
	else {
		fourcc_t     ssndId   = 'SSND';
		big_uint32_t ssndSize = static_cast<uint32_t>(m_dataSize + 8); // +8 for offset and blockSize fields
		big_uint32_t zero     = 0;
		m_file.write(reinterpret_cast<const char*>(&ssndId), 4);
		m_file.write(reinterpret_cast<const char*>(&ssndSize), 4);
		m_file.write(reinterpret_cast<const char*>(&zero), 4); // offset = 0
		m_file.write(reinterpret_cast<const char*>(&zero), 4); // blockSize = 0
	}

	m_file.flush();
}


////////////////////////////////////////////////////////////////////////////////
//	Constructors
////////////////////////////////////////////////////////////////////////////////

//	Open existing file.
AudioFile::AudioFile(const filesystem::path& path) : m_path(path) {
	if (!filesystem::exists(m_path))
		throw FileNotFound(m_path.string());
	if (!filesystem::is_regular_file(m_path))
		throw NotARegularFile(m_path.string());

	m_file.open(m_path.string(), ios_base::in | ios_base::out | ios_base::binary);
	if (m_file.fail())
		throw FileOpenError("Failed to open file: " + m_path.string());

	//	Read 12-byte container header
	m_file.read(reinterpret_cast<char*>(&m_header), 12);

	bool isRF64 = false;

	switch (m_header.id) {
		case 'RIFF':
			if (m_header.type != 'WAVE')
				throw UnsupportedFormat("Unknown RIFF media type.");
			m_type = AudioType::Wave;
			break;

		case 'RF64':
			if (m_header.type != 'WAVE')
				throw UnsupportedFormat("Unknown RF64 media type.");
			m_type = AudioType::Wave;
			isRF64 = true;
			break;

		case 'FORM':
			if (m_header.type != 'AIFF' && m_header.type != 'AIFC')
				throw UnsupportedFormat("Unknown FORM media type.");
			m_type = AudioType::Aiff;
			break;

		default:
			throw UnsupportedFormat("Unknown or unsupported file type.");
	}

	//	RF64 data size will be set by ds64 chunk
	int64_t rf64DataSize = -1;

	//	Iterate through chunks
	while (m_file.good()) {
		//	Read 8-byte chunk header: [ID 4][SIZE 4]
		uint8_t chunkHdr[8];
		m_file.read(reinterpret_cast<char*>(chunkHdr), 8);
		if (!m_file.good())
			break;

		fourcc_t chunkId;
		memcpy(&chunkId, chunkHdr, 4);

		uint32_t payloadSize;
		if (m_type == AudioType::Wave) {
			little_uint32_t ls;
			memcpy(&ls, chunkHdr + 4, 4);
			payloadSize = ls;
		}
		else {
			big_uint32_t bs;
			memcpy(&bs, chunkHdr + 4, 4);
			payloadSize = bs;
		}

		switch (chunkId) {
			case 'data': {
				//	WAVE audio data chunk
				m_dataStart = m_file.tellg();
				m_dataSize  = (isRF64 && rf64DataSize >= 0) ? rf64DataSize : payloadSize;
				//	Skip past audio payload
				m_file.seekg(m_dataSize, ios_base::cur);
				break;
			}

			case 'SSND': {
				//	AIFF audio data chunk — read offset and blockSize (8 more bytes)
				big_uint32_t ssndOffset, ssndBlockSize;
				m_file.read(reinterpret_cast<char*>(&ssndOffset), 4);
				m_file.read(reinterpret_cast<char*>(&ssndBlockSize), 4);

				uint32_t offsetVal = ssndOffset;
				m_dataStart        = static_cast<int64_t>(m_file.tellg()) + offsetVal;
				m_dataSize         = payloadSize - 8 - offsetVal; // SIZE - offset/blockSize fields - offset value

				//	Skip past audio payload
				m_file.seekg(payloadSize - 8, ios_base::cur);
				break;
			}

			case 'JUNK': case 'PAD ': case 'FREE': {
				//	Padding/alignment chunks — skip, not stored
				m_file.seekg(payloadSize, ios_base::cur);
				if (payloadSize & 1)
					m_file.seekg(1, ios_base::cur);
				break;
			}

			default: {
				//	Read the full chunk as a blob
				vector<uint8_t> blob(8 + payloadSize);
				memcpy(blob.data(), chunkHdr, 8);
				m_file.read(reinterpret_cast<char*>(blob.data() + 8), payloadSize);

				m_chunks.push_back(std::move(blob));

				//	For RF64: extract data size from ds64 chunk
				if (chunkId == 'ds64' && isRF64 && payloadSize >= 24) {
					little_int64_t ds64DataSize;
					memcpy(&ds64DataSize, m_chunks.back().data() + 16, 8);
					rf64DataSize = ds64DataSize;
				}

				//	Word-alignment: skip pad byte for odd payloads
				if (payloadSize & 1)
					m_file.seekg(1, ios_base::cur);
				break;
			}
		}
	}

	if (m_dataStart == 0)
		throw InvalidHeader("No audio data chunk found in file.");

	m_file.clear(); // clear EOF flag
}


//	Create new file (in-memory only until flush() is called).
AudioFile::AudioFile(const filesystem::path& path, AudioType type)
	: m_path(path), m_type(type)
{
	if (filesystem::exists(m_path))
		throw FileExists(m_path.string());

	if (m_type == AudioType::Unknown)
		throw UsageError("Must specify Wave or Aiff type.");

	if (m_type == AudioType::Wave) {
		m_header.id    = 'RIFF';
		m_header.lSize = 0;
		m_header.type  = 'WAVE';
	}
	else {
		m_header.id    = 'FORM';
		m_header.bSize = 0;
		m_header.type  = 'AIFF';
	}
}


//	Move constructor
AudioFile::AudioFile(AudioFile&& other) noexcept
	: m_path(std::move(other.m_path)),
	  m_type(other.m_type),
	  m_file(std::move(other.m_file)),
	  m_header(other.m_header),
	  m_chunks(std::move(other.m_chunks)),
	  m_dataStart(other.m_dataStart),
	  m_dataSize(other.m_dataSize),
	  m_readPos(other.m_readPos),
	  m_writePos(other.m_writePos),
	  m_dirty(other.m_dirty)
{
	other.m_type      = AudioType::Unknown;
	other.m_dataStart = 0;
	other.m_dataSize  = 0;
	other.m_readPos   = 0;
	other.m_writePos  = 0;
	other.m_dirty     = false;
}

//	Move assignment
AudioFile& AudioFile::operator=(AudioFile&& other) noexcept {
	if (this != &other) {
		//	Flush current state if dirty
		if (m_dirty) {
			try { flush(); }
			catch (...) {}
		}

		m_path      = std::move(other.m_path);
		m_type      = other.m_type;
		m_file      = std::move(other.m_file);
		m_header    = other.m_header;
		m_chunks    = std::move(other.m_chunks);
		m_dataStart = other.m_dataStart;
		m_dataSize  = other.m_dataSize;
		m_readPos   = other.m_readPos;
		m_writePos  = other.m_writePos;
		m_dirty     = other.m_dirty;

		other.m_type      = AudioType::Unknown;
		other.m_dataStart = 0;
		other.m_dataSize  = 0;
		other.m_readPos   = 0;
		other.m_writePos  = 0;
		other.m_dirty     = false;
	}
	return *this;
}


//	Destructor
AudioFile::~AudioFile() {
	if (m_dirty) {
		try { flush(); }
		catch (...) {}
	}
}


////////////////////////////////////////////////////////////////////////////////
//	Chunk management
////////////////////////////////////////////////////////////////////////////////

bool AudioFile::isMultiAllowed(fourcc_t id) {
	return id == 'ANNO' || id == 'MIDI' || id == 'APPL' || id == 'SAXL'
	    || id == 'JUNK' || id == 'PAD ' || id == 'FREE';
}

size_t AudioFile::addChunk(const void* data) {
	auto ptr = static_cast<const uint8_t*>(data);

	fourcc_t id;
	memcpy(&id, ptr, 4);

	if (!isMultiAllowed(id) && findChunk(id).has_value())
		throw UsageError("Duplicate singleton chunk.");

	uint32_t payloadSize;
	if (m_type == AudioType::Wave) {
		little_uint32_t ls;
		memcpy(&ls, ptr + 4, 4);
		payloadSize = ls;
	}
	else {
		big_uint32_t bs;
		memcpy(&bs, ptr + 4, 4);
		payloadSize = bs;
	}

	size_t totalSize = payloadSize + 8;
	m_chunks.emplace_back(ptr, ptr + totalSize);
	return m_chunks.size() - 1;
}

size_t AudioFile::chunkCount() const {
	return m_chunks.size();
}

span<const uint8_t> AudioFile::chunk(size_t index) const {
	return m_chunks.at(index);
}

void AudioFile::replaceChunk(size_t index, const void* data) {
	auto ptr = static_cast<const uint8_t*>(data);

	uint32_t payloadSize;
	if (m_type == AudioType::Wave) {
		little_uint32_t ls;
		memcpy(&ls, ptr + 4, 4);
		payloadSize = ls;
	}
	else {
		big_uint32_t bs;
		memcpy(&bs, ptr + 4, 4);
		payloadSize = bs;
	}

	size_t totalSize = payloadSize + 8;
	m_chunks.at(index).assign(ptr, ptr + totalSize);
}

void AudioFile::deleteChunk(size_t index) {
	if (index >= m_chunks.size())
		throw out_of_range("Chunk index out of range.");
	m_chunks.erase(m_chunks.begin() + static_cast<ptrdiff_t>(index));
}


////////////////////////////////////////////////////////////////////////////////
//	Audio data I/O
////////////////////////////////////////////////////////////////////////////////

streamsize AudioFile::read(char* buf, streamsize count) {
	if (m_dataStart == 0)
		throw UsageError("File structure not established. Call flush() first.");

	//	Clamp to available data
	int64_t available = m_dataSize - m_readPos;
	if (available <= 0)
		return 0;
	if (count > available)
		count = static_cast<streamsize>(available);

	m_file.clear();
	m_file.seekg(m_dataStart + m_readPos, ios_base::beg);
	m_file.read(buf, count);

	auto bytesRead = m_file.gcount();
	m_readPos      += bytesRead;
	return bytesRead;
}

streamsize AudioFile::write(const char* buf, streamsize count) {
	if (m_dataStart == 0)
		throw UsageError("File structure not established. Call flush() first.");

	m_file.clear();
	m_file.seekp(m_dataStart + m_writePos, ios_base::beg);
	m_file.write(buf, count);

	m_writePos += count;
	if (m_writePos > m_dataSize)
		m_dataSize = m_writePos;

	m_dirty = true;
	return count;
}

void AudioFile::seekg(streamoff off, ios_base::seekdir dir) {
	switch (dir) {
		case ios_base::beg:
			m_readPos = off;
			break;
		case ios_base::cur:
			m_readPos += off;
			break;
		case ios_base::end:
			m_readPos = m_dataSize + off;
			break;
	}

	if (m_readPos < 0) m_readPos = 0;
	if (m_readPos > m_dataSize) m_readPos = m_dataSize;
}

void AudioFile::seekp(streamoff off, ios_base::seekdir dir) {
	switch (dir) {
		case ios_base::beg:
			m_writePos = off;
			break;
		case ios_base::cur:
			m_writePos += off;
			break;
		case ios_base::end:
			m_writePos = m_dataSize + off;
			break;
	}

	if (m_writePos < 0) m_writePos = 0;
	if (m_writePos > m_dataSize) m_writePos = m_dataSize;
}

streampos AudioFile::tellg() const {
	return m_readPos;
}

streampos AudioFile::tellp() const {
	return m_writePos;
}


////////////////////////////////////////////////////////////////////////////////
//	File operations
////////////////////////////////////////////////////////////////////////////////

void AudioFile::flush() {
	if (m_dataStart == 0) {
		//	First flush: create the file and establish header structure.
		m_file.open(m_path.string(), ios_base::in | ios_base::out | ios_base::binary | ios_base::trunc);
		if (m_file.fail())
			throw FileOpenError("Failed to create file: " + m_path.string());

		const int64_t dataHdrSize = (m_type == AudioType::Wave) ? 8 : 16;

		//	Minimum overhead: container(12) + chunks + alignment hdr(8) + data hdr
		int64_t overhead = 12 + allChunksSize() + 8 + dataHdrSize;

		//	Choose base alignment
		int64_t alignment = (m_type == AudioType::Wave) ? 4096 : 512;
		while (alignment < overhead)
			alignment *= 2;

		m_dataStart = alignment;
	}
	else {
		//	Verify chunks still fit
		const int64_t dataHdrSize = (m_type == AudioType::Wave) ? 8 : 16;
		int64_t       overhead    = 12 + allChunksSize() + 8 + dataHdrSize;
		if (overhead > m_dataStart)
			throw FormatError("Chunks exceed allocated header space.");
	}

	writeHeaders();
	m_dirty = false;
}


////////////////////////////////////////////////////////////////////////////////
//	Queries
////////////////////////////////////////////////////////////////////////////////

bool AudioFile::isLittleEndian() const {
	return m_type == AudioType::Wave;
}

AudioType AudioFile::type() const {
	return m_type;
}

int64_t AudioFile::dataSize() const {
	return m_dataSize;
}

const filesystem::path& AudioFile::path() const {
	return m_path;
}

void AudioFile::setContainerType(fourcc_t type) {
	m_header.type = type;
}


////////////////////////////////////////////////////////////////////////////////
//	Chunk lookup
////////////////////////////////////////////////////////////////////////////////

optional<size_t> AudioFile::findChunk(fourcc_t id) const {
	for (size_t i = 0; i < m_chunks.size(); ++i) {
		if (m_chunks[i].size() >= 4) {
			fourcc_t chunkId;
			memcpy(&chunkId, m_chunks[i].data(), 4);
			if (chunkId == id)
				return i;
		}
	}
	return nullopt;
}

void AudioFile::setChunk(vector<uint8_t> blob) {
	if (blob.size() < 8)
		throw FormatError("Chunk must be at least 8 bytes (ID + SIZE).");

	fourcc_t id;
	memcpy(&id, blob.data(), 4);

	auto idx = findChunk(id);
	if (idx.has_value())
		m_chunks[*idx] = std::move(blob);
	else
		m_chunks.push_back(std::move(blob));
}

} // namespace Diskerror
