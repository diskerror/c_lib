//
// Created by Reid Woodbury.
//

#include "AudioFile.h"
#include "DiskerrorExceptions.h"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <algorithm>
#include <cstring>

#include "BigFloat80.h"

namespace Diskerror {

using namespace std;
using namespace boost;
using namespace boost::endian;

//  Convert a four-char-code (big endian 32-bit integer) to a string of 4 characters.
inline string fourcc2str(fourcc_t fcc) {
	char buf[5] = {0};
	uint32_t val = fcc; // already big-endian
	memcpy(buf, &val, 4);
	return string(buf);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Chunk Implementations
////////////////////////////////////////////////////////////////////////////////////////////////////

std::vector<uint8_t> Chunk::serialize(bool isLittleEndian) const {
	std::vector<uint8_t> payload = serializePayload(isLittleEndian);
	uint32_t size = static_cast<uint32_t>(payload.size());

	std::vector<uint8_t> result;
	result.resize(8 + size); // Header + Payload

	// Header
	uint32_t idVal = id; // ID is always big-endian (FourCC)
	memcpy(result.data(), &idVal, 4);

	if (isLittleEndian) {
		little_uint32_t s = size;
		memcpy(result.data() + 4, &s, 4);
	} else {
		big_uint32_t s = size;
		memcpy(result.data() + 4, &s, 4);
	}

	// Payload
	memcpy(result.data() + 8, payload.data(), size);

	return result;
}

std::vector<uint8_t> WaveFmtChunk::serializePayload(bool isLittleEndian) const {
	std::vector<uint8_t> data(16 + extension.size());

	if (isLittleEndian) {
		little_uint16_t ft = formatTag;
		little_uint16_t nc = numChannels;
		little_uint32_t sr = sampleRate;
		little_uint32_t bps = bytesPerSec;
		little_uint16_t ba = blockAlign;
		little_uint16_t bips = bitsPerSample;

		memcpy(data.data(), &ft, 2);
		memcpy(data.data() + 2, &nc, 2);
		memcpy(data.data() + 4, &sr, 4);
		memcpy(data.data() + 8, &bps, 4);
		memcpy(data.data() + 12, &ba, 2);
		memcpy(data.data() + 14, &bips, 2);
	} else {
		// Should not happen for WAVE, but for completeness
		big_uint16_t ft = formatTag;
		big_uint16_t nc = numChannels;
		big_uint32_t sr = sampleRate;
		big_uint32_t bps = bytesPerSec;
		big_uint16_t ba = blockAlign;
		big_uint16_t bips = bitsPerSample;

		memcpy(data.data(), &ft, 2);
		memcpy(data.data() + 2, &nc, 2);
		memcpy(data.data() + 4, &sr, 4);
		memcpy(data.data() + 8, &bps, 4);
		memcpy(data.data() + 12, &ba, 2);
		memcpy(data.data() + 14, &bips, 2);
	}

	if (!extension.empty()) {
		memcpy(data.data() + 16, extension.data(), extension.size());
	}

	return data;
}

std::vector<uint8_t> AiffCommChunk::serializePayload(bool isLittleEndian) const {
	// AIFF is Big Endian
	std::vector<uint8_t> data(18 + (compressionName.empty() ? 0 : 4 + 1 + compressionName.size() + (compressionName.size() % 2)));

	big_uint16_t nc = numChannels;
	big_uint32_t nsf = numSampleFrames;
	big_uint16_t ss = sampleSize;

	memcpy(data.data(), &nc, 2);
	memcpy(data.data() + 2, &nsf, 4);
	memcpy(data.data() + 6, &ss, 2);

	BigFloat80 bf80(sampleRate);
	memcpy(data.data() + 8, bf80.rawData(), 10);

	if (!compressionName.empty() || compressionType != 0) {
		size_t offset = 18;
		uint32_t ct = compressionType; // FourCC is big endian
		memcpy(data.data() + offset, &ct, 4);
		offset += 4;

		uint8_t len = static_cast<uint8_t>(compressionName.size());
		memcpy(data.data() + offset, &len, 1);
		offset += 1;

		memcpy(data.data() + offset, compressionName.data(), len);
		offset += len;

		if (len % 2 == 0) { // Pad byte if even length (total size must be even, pstring includes count byte)
			data[offset] = 0;
		}
	}

	return data;
}

std::vector<uint8_t> DataChunk::serializePayload(bool isLittleEndian) const {
	// This only serializes the header part of the payload (offset/blockSize for AIFF)
	// The actual audio data is handled separately during flush/write
	if (id == fourcc_t('SSND')) {
		std::vector<uint8_t> data(8);
		big_uint32_t off = offset;
		big_uint32_t bs = blockSize;
		memcpy(data.data(), &off, 4);
		memcpy(data.data() + 4, &bs, 4);
		return data;
	}
	return {}; // WAVE 'data' chunk has no prefix in payload
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// AudioFile Implementation
////////////////////////////////////////////////////////////////////////////////////////////////////

AudioFile::AudioFile(const filesystem::path& fPath, ios_base::openmode mode) : the_file_path(fPath) {
	if (!filesystem::exists(this->the_file_path)) throw FileNotFound(this->the_file_path.string());
	if (!filesystem::is_regular_file(this->the_file_path)) throw NotARegularFile(this->the_file_path.string());

	// Always need binary
	mode |= ios_base::binary;

	this->file_access.open(this->the_file_path.string(), mode);
	if (this->file_access.fail())
		throw FileOpenError("There was a problem opening the input file.");

	this->parseHeader();
}

AudioFile::AudioFile(
	const filesystem::path& fPath,
	baseAudioFileType_t     type,
	uint32_t                sampleRate,
	uint16_t                numChannels,
	uint16_t                bitsPerSample,
	bool                    isFloat
) : the_file_path(fPath), base_type(type) {
	if (filesystem::exists(this->the_file_path))
		throw FileExists(this->the_file_path.string());

	// Create empty file
	this->file_access.open(this->the_file_path.string(), ios_base::in | ios_base::out | ios_base::trunc | ios_base::binary);
	if (this->file_access.fail())
		throw FileOpenError("Could not create file.");

	format.sampleRate = sampleRate;
	format.numChannels = numChannels;
	format.bitsPerSample = bitsPerSample;
	format.bytesPerFrame = (numChannels * bitsPerSample + 7) / 8; // rounded up
	format.isFloatingPoint = isFloat;
	format.isLittleEndian = (type == BASE_TYPE_WAVE);
	format.encoding = isFloat ? fourcc_t('IEEE') : fourcc_t('PCM ');

    if (type == BASE_TYPE_WAVE) {
        auto fmt = std::make_shared<WaveFmtChunk>();
        fmt->sampleRate = sampleRate;
        fmt->numChannels = numChannels;
        fmt->bitsPerSample = bitsPerSample;
        fmt->blockAlign = format.bytesPerFrame;
        fmt->bytesPerSec = sampleRate * format.bytesPerFrame;
        fmt->formatTag = isFloat ? 0x0003 : 0x0001;
        m_formatChunk = fmt;
        m_chunks.push_back(fmt);

        m_dataChunk = std::make_shared<DataChunk>(fourcc_t('data'));
        m_chunks.push_back(m_dataChunk);
    } else if (type == BASE_TYPE_AIFF) {
        auto comm = std::make_shared<AiffCommChunk>();
        comm->sampleRate = sampleRate;
        comm->numChannels = numChannels;
        comm->sampleSize = bitsPerSample;
        comm->numSampleFrames = 0;
        m_formatChunk = comm;
        m_chunks.push_back(comm);

        m_dataChunk = std::make_shared<DataChunk>(fourcc_t('SSND'));
        m_chunks.push_back(m_dataChunk);
    } else {
        throw UnsupportedFormat("Unsupported base file type for creation.");
    }
}

AudioFile::~AudioFile() {
	if (file_access.is_open()) {
		try {
			flush();
		} catch (...) {}
		file_access.close();
	}
}

AudioFile::AudioFile(AudioFile&& other) noexcept
    : the_file_path(std::move(other.the_file_path)),
      file_access(std::move(other.file_access)),
      base_type(other.base_type),
      format(other.format),
      m_chunks(std::move(other.m_chunks)),
      m_dataChunk(std::move(other.m_dataChunk)),
      m_formatChunk(std::move(other.m_formatChunk)),
      m_readPos(other.m_readPos),
      m_writePos(other.m_writePos)
{
    // Invalidate other
    other.base_type = BASE_TYPE_UNKNOWN;
}

AudioFile& AudioFile::operator=(AudioFile&& other) noexcept {
    if (this != &other) {
        // Clean up current
        if (file_access.is_open()) {
            try { flush(); } catch(...) {}
            file_access.close();
        }

        const_cast<filesystem::path&>(the_file_path) = std::move(other.the_file_path);
        file_access = std::move(other.file_access);
        base_type = other.base_type;
        format = other.format;
        m_chunks = std::move(other.m_chunks);
        m_dataChunk = std::move(other.m_dataChunk);
        m_formatChunk = std::move(other.m_formatChunk);
        m_readPos = other.m_readPos;
        m_writePos = other.m_writePos;

        other.base_type = BASE_TYPE_UNKNOWN;
    }
    return *this;
}

std::unique_ptr<AudioFile> AudioFile::Builder::build() {
    if (m_path.empty()) throw std::invalid_argument("Path cannot be empty.");
    return std::make_unique<AudioFile>(
        m_path,
        m_type,
        m_sampleRate,
        m_numChannels,
        m_bitsPerSample,
        m_isFloat
    );
}

bool AudioFile::isReserved(fourcc_t id) const {
	if (base_type == BASE_TYPE_WAVE) {
		return (id == fourcc_t('RIFF') || id == fourcc_t('RF64') || id == fourcc_t('fmt ') ||
				id == fourcc_t('data') || id == fourcc_t('ds64') || id == fourcc_t('fact') || id == fourcc_t('JUNK'));
	}

	return (id == fourcc_t('FORM') || id == fourcc_t('COMM') || id == fourcc_t('SSND') ||
			id == fourcc_t('FVER') || id == fourcc_t('PAD '));
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void AudioFile::parseHeader() {
	m_chunks.clear();
	m_readPos = 0;
	m_writePos = 0;

	fourcc_t id;
	file_access.seekg(0);
	file_access.read(reinterpret_cast<char*>(&id), 4);
	file_access.seekg(0);

	if (id == fourcc_t('RIFF') || id == fourcc_t('RF64')) {
		parseWAVE();
	} else if (id == fourcc_t('FORM')) {
		parseAIFF();
	} else {
		throw InvalidHeader("Unknown file header: " + fourcc2str(id));
	}
    syncFormatFromChunks();
}

void AudioFile::parseWAVE() {
	waveFileHeader_t header;
	file_access.read(reinterpret_cast<char*>(&header), 12);

	if (header.type != fourcc_t('WAVE')) throw InvalidHeader("Not a WAVE file.");

	base_type = BASE_TYPE_WAVE;

	while (file_access) {
		fourcc_t chunkID;
		little_uint32_t chunkSize;

		file_access.read(reinterpret_cast<char*>(&chunkID), 4);
		if (file_access.gcount() < 4) break;

		file_access.read(reinterpret_cast<char*>(&chunkSize), 4);
		if (file_access.gcount() < 4) break;

		uint64_t size = chunkSize;
        uint64_t startOfPayload = file_access.tellg();

		if (chunkID == fourcc_t('fmt ')) {
			auto chunk = std::make_shared<WaveFmtChunk>();
            file_access.read(reinterpret_cast<char*>(&chunk->formatTag), 2);
            file_access.read(reinterpret_cast<char*>(&chunk->numChannels), 2);
            file_access.read(reinterpret_cast<char*>(&chunk->sampleRate), 4);
            file_access.read(reinterpret_cast<char*>(&chunk->bytesPerSec), 4);
            file_access.read(reinterpret_cast<char*>(&chunk->blockAlign), 2);
            file_access.read(reinterpret_cast<char*>(&chunk->bitsPerSample), 2);

            if (size > 16) {
                uint16_t cbSize;
                file_access.read(reinterpret_cast<char*>(&cbSize), 2);
                chunk->extension.resize(cbSize);
                file_access.read(reinterpret_cast<char*>(chunk->extension.data()), cbSize);
            }
            m_formatChunk = chunk;
			m_chunks.push_back(chunk);
		}
		else if (chunkID == fourcc_t('data')) {
            m_dataChunk = std::make_shared<DataChunk>(fourcc_t('data'));
            m_dataChunk->fileOffset = startOfPayload;
			m_dataChunk->payloadSize = static_cast<uint32_t>(size);

			if (size == 0xFFFFFFFF) {
				for(auto& c : m_chunks) {
					if (c->id == fourcc_t('ds64')) {
						auto unk = std::dynamic_pointer_cast<UnknownChunk>(c);
                        if (unk && unk->payload.size() >= 16) {
                            memcpy(&m_dataChunk->payloadSize, unk->payload.data() + 8, 8);
                        }
						break;
					}
				}
			}
			m_chunks.push_back(m_dataChunk);
			file_access.seekg(startOfPayload + size, ios_base::beg);
		}
        else if (chunkID == fourcc_t('JUNK')) {
            file_access.seekg(startOfPayload + size, ios_base::beg);
        }
		else {
			std::vector<uint8_t> payload(size);
			if (size > 0) {
				file_access.read(reinterpret_cast<char*>(payload.data()), size);
			}
            auto chunk = std::make_shared<UnknownChunk>(chunkID, payload);
			m_chunks.push_back(chunk);
		}

		if (size % 2 != 0) {
			file_access.seekg(1, ios_base::cur);
		}
	}
	m_readPos = 0;
	m_writePos = 0;
}

void AudioFile::parseAIFF() {
	aiffFileHeader_t header;
	file_access.read(reinterpret_cast<char*>(&header), 12);

	if (header.type != fourcc_t('AIFF') && header.type != fourcc_t('AIFC')) throw InvalidHeader("Not an AIFF file.");
	base_type = BASE_TYPE_AIFF;

	while (file_access) {
		fourcc_t chunkID;
		big_uint32_t chunkSize;

		file_access.read(reinterpret_cast<char*>(&chunkID), 4);
		if (file_access.gcount() < 4) break;

		file_access.read(reinterpret_cast<char*>(&chunkSize), 4);
		if (file_access.gcount() < 4) break;

		uint64_t size = chunkSize;
        uint64_t startOfPayload = file_access.tellg();

		if (chunkID == fourcc_t('COMM')) {
			auto chunk = std::make_shared<AiffCommChunk>();
            file_access.read(reinterpret_cast<char*>(&chunk->numChannels), 2);
            file_access.read(reinterpret_cast<char*>(&chunk->numSampleFrames), 4);
            file_access.read(reinterpret_cast<char*>(&chunk->sampleSize), 2);

            uint8_t exp[10];
            file_access.read(reinterpret_cast<char*>(exp), 10);
            BigFloat80 bf80(reinterpret_cast<const char*>(exp));
            chunk->sampleRate = bf80.toDouble();

            if (size > 18) {
                file_access.read(reinterpret_cast<char*>(&chunk->compressionType), 4);
                uint8_t nameLen;
                file_access.read(reinterpret_cast<char*>(&nameLen), 1);
                chunk->compressionName.resize(nameLen);
                file_access.read(reinterpret_cast<char*>(chunk->compressionName.data()), nameLen);
            }

            m_formatChunk = chunk;
			m_chunks.push_back(chunk);
		}
		else if (chunkID == fourcc_t('SSND')) {
            m_dataChunk = std::make_shared<DataChunk>(fourcc_t('SSND'));
            file_access.read(reinterpret_cast<char*>(&m_dataChunk->offset), 4);
            file_access.read(reinterpret_cast<char*>(&m_dataChunk->blockSize), 4);

			m_dataChunk->fileOffset = (uint64_t)file_access.tellg() + (uint32_t)m_dataChunk->offset;
			m_dataChunk->payloadSize = static_cast<uint32_t>(size) - 8 - (uint32_t)m_dataChunk->offset;

            m_chunks.push_back(m_dataChunk);
			file_access.seekg(startOfPayload + size, ios_base::beg);
		}
        else if (chunkID == fourcc_t('PAD ')) {
            file_access.seekg(startOfPayload + size, ios_base::beg);
        }
		else {
			std::vector<uint8_t> payload(size);
			if (size > 0) {
				file_access.read(reinterpret_cast<char*>(payload.data()), size);
			}
            auto chunk = std::make_shared<UnknownChunk>(chunkID, payload);
			m_chunks.push_back(chunk);
		}

		if (size % 2 != 0) {
			file_access.seekg(1, ios_base::cur);
		}
	}
	m_readPos = 0;
	m_writePos = 0;
}

void AudioFile::syncFormatFromChunks() {
    if (auto waveFmt = std::dynamic_pointer_cast<WaveFmtChunk>(m_formatChunk)) {
        format.sampleRate = waveFmt->sampleRate;
        format.numChannels = waveFmt->numChannels;
        format.bitsPerSample = waveFmt->bitsPerSample;
        format.bytesPerFrame = waveFmt->blockAlign;
        format.isFloatingPoint = (waveFmt->formatTag == 0x0003);
        format.isLittleEndian = true;
        format.encoding = format.isFloatingPoint ? fourcc_t('IEEE') : fourcc_t('PCM ');
    } else if (auto aiffComm = std::dynamic_pointer_cast<AiffCommChunk>(m_formatChunk)) {
        format.sampleRate = static_cast<uint32_t>(aiffComm->sampleRate);
        format.numChannels = aiffComm->numChannels;
        format.bitsPerSample = aiffComm->sampleSize;
        format.bytesPerFrame = (format.numChannels * format.bitsPerSample + 7) / 8;
        format.isFloatingPoint = (aiffComm->compressionType == fourcc_t('fl32') || aiffComm->compressionType == fourcc_t('FL32'));
        format.isLittleEndian = (aiffComm->compressionType == fourcc_t('sowt') || aiffComm->compressionType == fourcc_t('swot'));
        format.encoding = (aiffComm->compressionType == fourcc_t(0)) ? fourcc_t('PCM ') : aiffComm->compressionType;
    }
}

string AudioFile::getFileName() const {
	return string(this->the_file_path.filename());
}

baseAudioFileType_t AudioFile::getBaseType() const {
	return this->base_type;
}

const AudioFormat& AudioFile::getFormat() const {
	return this->format;
}

int64_t AudioFile::getNumFrames() const {
	if (format.bytesPerFrame == 0) return 0;
	return m_dataChunk->payloadSize / format.bytesPerFrame;
}

void AudioFile::seekg(int64_t offset, ios_base::seekdir dir) {
	int64_t target = 0;
	switch (dir) {
		case ios_base::beg: target = offset; break;
		case ios_base::cur: target = m_readPos + offset; break;
		case ios_base::end: target = m_dataChunk->payloadSize + offset; break;
	}
	if (target < 0) target = 0;
	m_readPos = target;
}

uint64_t AudioFile::tellg() { return m_readPos; }

void AudioFile::seekp(int64_t offset, ios_base::seekdir dir) {
	int64_t target = 0;
	switch (dir) {
		case ios_base::beg: target = offset; break;
		case ios_base::cur: target = m_writePos + offset; break;
		case ios_base::end: target = m_dataChunk->payloadSize + offset; break;
	}
	if (target < 0) target = 0;
	m_writePos = target;
}

uint64_t AudioFile::tellp() { return m_writePos; }

void AudioFile::read(void* buffer, size_t size) {
	if (m_readPos >= m_dataChunk->payloadSize) return;

    // If we have data in memory, read from there (it's the most up-to-date)
    if (!m_dataChunk->memoryBuffer.empty()) {
        size_t toRead = size;
        if (m_readPos + size > m_dataChunk->memoryBuffer.size()) {
            toRead = m_dataChunk->memoryBuffer.size() - m_readPos;
        }
        memcpy(buffer, m_dataChunk->memoryBuffer.data() + m_readPos, toRead);
        m_readPos += toRead;
        return;
    }

    // Otherwise read from file
	file_access.clear();
	file_access.seekg(m_dataChunk->fileOffset + (std::streamoff)m_readPos);
	size_t toRead = size;
	if (m_readPos + size > m_dataChunk->payloadSize) toRead = m_dataChunk->payloadSize - m_readPos;
	file_access.read(reinterpret_cast<char*>(buffer), toRead);
	m_readPos += file_access.gcount();
}

void AudioFile::write(const void* buffer, size_t size) {
    // If memory buffer is empty but we have file data, we MUST load it first
    // because we can't safely modify the file in-place/append without breaking 'flush'
    if (m_dataChunk->memoryBuffer.empty() && m_dataChunk->payloadSize > 0 && m_dataChunk->fileOffset > 0) {
        m_dataChunk->memoryBuffer.resize(m_dataChunk->payloadSize);
        file_access.clear();
        file_access.seekg(m_dataChunk->fileOffset);
        file_access.read(reinterpret_cast<char*>(m_dataChunk->memoryBuffer.data()), m_dataChunk->payloadSize);
        // Disable file offset to force using memory buffer
        m_dataChunk->fileOffset = 0;
    }

    // Ensure memory buffer is initialized if it was empty (new file or empty data)
    if (m_dataChunk->memoryBuffer.empty() && size > 0) {
        m_dataChunk->fileOffset = 0;
    }

    // Resize if writing past end
    if (m_writePos + size > m_dataChunk->memoryBuffer.size()) {
        m_dataChunk->memoryBuffer.resize(m_writePos + size);
    }

    // Perform write
    memcpy(m_dataChunk->memoryBuffer.data() + m_writePos, buffer, size);
    m_writePos += size;

    // Update payload size
    if (m_dataChunk->memoryBuffer.size() > m_dataChunk->payloadSize) {
        m_dataChunk->payloadSize = static_cast<uint32_t>(m_dataChunk->memoryBuffer.size());
    }
}

uint32_t AudioFile::calculatePadding(uint64_t currentOffset, uint32_t alignment, uint32_t nextChunkHeaderSize) const {
    uint64_t payloadStart = currentOffset + 8 + nextChunkHeaderSize;
    uint32_t r = payloadStart % alignment;
    if (r == 0) return 0;
    return alignment - r;
}

void AudioFile::flush() {
	if (!file_access.is_open()) return;

    filesystem::path tempPath = the_file_path;
    tempPath.replace_extension(".tmp");

    ofstream out(tempPath, ios_base::binary);
    if (!out) throw FileOpenError("Could not create temporary file for flush.");

    bool isLittle = (base_type == BASE_TYPE_WAVE);

    if (base_type == BASE_TYPE_WAVE) {
        out.write("RIFF\0\0\0\0WAVE", 12);
    } else {
        out.write("FORM\0\0\0\0AIFF", 12);
    }

    for (auto& c : m_chunks) {
        if (c == m_dataChunk) continue;
        auto bytes = c->serialize(isLittle);
        out.write(reinterpret_cast<char*>(bytes.data()), bytes.size());
        if (bytes.size() % 2 != 0) out.put(0);
    }

    uint64_t currentPos = out.tellp();
    uint32_t align = (base_type == BASE_TYPE_WAVE) ? WAVE_ALIGNMENT : AIFF_ALIGNMENT;
    uint32_t nextHeaderSize = (base_type == BASE_TYPE_WAVE) ? 8 : 16;
    uint32_t padding = calculatePadding(currentPos, align, nextHeaderSize);

    if (padding > 0) {
        fourcc_t padID = (base_type == BASE_TYPE_WAVE) ? fourcc_t('JUNK') : fourcc_t('PAD ');
        std::vector<uint8_t> padPayload(padding, 0);
        UnknownChunk padChunk(padID, padPayload);
        auto bytes = padChunk.serialize(isLittle);
        out.write(reinterpret_cast<char*>(bytes.data()), bytes.size());
    }

    uint64_t newDataHeaderOffset = out.tellp(); (void)newDataHeaderOffset;
    auto dataHeaderBytes = m_dataChunk->serialize(isLittle);
    out.write(reinterpret_cast<char*>(dataHeaderBytes.data()), dataHeaderBytes.size());
    uint64_t newDataOffset = out.tellp();

    if (!m_dataChunk->memoryBuffer.empty()) {
        out.write(reinterpret_cast<char*>(m_dataChunk->memoryBuffer.data()), m_dataChunk->memoryBuffer.size());
    } else if (m_dataChunk->fileOffset > 0) {
        file_access.clear();
        file_access.seekg(m_dataChunk->fileOffset);
        std::vector<char> buffer(65536);
        uint64_t remaining = m_dataChunk->payloadSize;
        while (remaining > 0) {
            uint64_t toRead = std::min(remaining, (uint64_t)buffer.size());
            file_access.read(buffer.data(), toRead);
            out.write(buffer.data(), file_access.gcount());
            remaining -= file_access.gcount();
        }
    }
    if (m_dataChunk->payloadSize % 2 != 0) out.put(0);

    uint64_t finalSize = out.tellp();

    out.seekp(4);
    uint32_t riffSize = (uint32_t)(finalSize - 8);
    if (isLittle) native_to_little_inplace(riffSize);
    else native_to_big_inplace(riffSize);
    out.write(reinterpret_cast<char*>(&riffSize), 4);

    out.close();
    file_access.close();

    filesystem::rename(tempPath, the_file_path);

    file_access.open(the_file_path, ios_base::in | ios_base::out | ios_base::binary);
    m_dataChunk->fileOffset = newDataOffset;
}

std::vector<ChunkInfo> AudioFile::chunkList() const {
	std::vector<ChunkInfo> list;
	size_t idx = 0;
	for (const auto& chunk : m_chunks) {
		// Calculate size (approximate for now, or we could serialize to check)
		// For simplicity, we might need to store size or calculate it.
		// Let's serialize payload to get size.
		auto payload = chunk->serializePayload(base_type == BASE_TYPE_WAVE);
		list.push_back({chunk->id, (uint32_t)payload.size(), idx++});
	}
	return list;
}

size_t AudioFile::getChunkCount(fourcc_t id) const {
	size_t count = 0;
	for (const auto& chunk : m_chunks) { if (chunk->id == id) count++; }
	return count;
}

std::vector<unsigned char> AudioFile::getChunk(fourcc_t id, size_t index) {
	size_t current = 0;
	for (const auto& chunk : m_chunks) {
		if (chunk->id == id) {
			if (current == index) {
                auto bytes = chunk->serialize(base_type == BASE_TYPE_WAVE);
                if (bytes.size() <= 8) return {};
                return std::vector<unsigned char>(bytes.begin() + 8, bytes.end());
			}
			current++;
		}
	}
	return {};
}

void AudioFile::addChunk(fourcc_t id, const std::vector<unsigned char>& payload) {
	if (isReserved(id)) throw ReservedChunkError("Cannot add reserved chunk type.");
    auto chunk = std::make_shared<UnknownChunk>(id, payload);
	m_chunks.push_back(chunk);
}

void AudioFile::addChunk(fourcc_t id, const void* data, size_t size) {
	std::vector<unsigned char> v(size);
	memcpy(v.data(), data, size);
	addChunk(id, v);
}

void AudioFile::deleteChunk(fourcc_t id, size_t index) {
	if (isReserved(id)) throw ReservedChunkError("Cannot delete reserved chunk type.");
	size_t current = 0;
	for (auto it = m_chunks.begin(); it != m_chunks.end(); ++it) {
		if ((*it)->id == id) {
			if (current == index) { m_chunks.erase(it); return; }
			current++;
		}
	}
}

void AudioFile::updateAudioData(const std::vector<uint8_t>& pcmData) {
    if (!m_dataChunk) throw FormatError("No data chunk found to update!");

    // Store new data
    m_dataChunk->memoryBuffer = pcmData;
    m_dataChunk->payloadSize = static_cast<uint32_t>(pcmData.size());

    // Invalidate file offset so flush() uses the memory buffer
    m_dataChunk->fileOffset = 0;

    // Reset cursor
    m_readPos = 0;
    m_writePos = 0;
}

void AudioFile::updateFormat(uint32_t sampleRate, uint16_t numChannels, uint16_t bitsPerSample, bool isFloat) {
    if (base_type == BASE_TYPE_WAVE) {
        auto fmt = std::dynamic_pointer_cast<WaveFmtChunk>(m_formatChunk);
        if (!fmt) throw FormatError("Format chunk is missing or invalid type.");

        fmt->sampleRate = sampleRate;
        fmt->numChannels = numChannels;
        fmt->bitsPerSample = bitsPerSample;
        fmt->blockAlign = (numChannels * bitsPerSample + 7) / 8;
        fmt->bytesPerSec = sampleRate * fmt->blockAlign;
        fmt->formatTag = isFloat ? 0x0003 : 0x0001;

    } else if (base_type == BASE_TYPE_AIFF) {
        auto comm = std::dynamic_pointer_cast<AiffCommChunk>(m_formatChunk);
        if (!comm) throw FormatError("Format chunk is missing or invalid type.");

        comm->sampleRate = sampleRate;
        comm->numChannels = numChannels;
        comm->sampleSize = bitsPerSample;
        // Note: numSampleFrames should be updated based on data size,
        // but that requires knowing the data size.
        // We assume updateAudioData will be called or has been called?
        // Actually, we can calculate it from m_dataChunk if updated.
        if (m_dataChunk->payloadSize > 0 && comm->numChannels > 0 && comm->sampleSize > 0) {
             uint32_t bytesPerFrame = (comm->numChannels * comm->sampleSize + 7) / 8;
             comm->numSampleFrames = m_dataChunk->payloadSize / bytesPerFrame;
        }

        comm->compressionType = isFloat ? fourcc_t('fl32') : fourcc_t(0);
        comm->compressionName = isFloat ? "32-bit Floating Point" : "";
    }

    syncFormatFromChunks();
}

} //  namespace Diskerror