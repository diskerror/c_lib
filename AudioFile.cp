//
// Created by Reid Woodbury.
//

#include "AudioFile.h"

#include <format>
#include <functional>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

namespace Diskerror {

using namespace std;
using namespace boost;
using namespace boost::endian;

//  Convert a four-char-code (big endian 32-bit integer) to a string of 4 characters.
inline string fourcc2str(fourcc_t fcc) {
	return string(reinterpret_cast<const char*>(&fcc), 4);
}

inline fourcc_t short2hexFourcc(const uint16_t in) {
	string   s = format("{:04X}", in);
	fourcc_t fcc;
	auto     fccp = reinterpret_cast<char*>(&fcc);
	for (uint16_t i = 0; i < 4; i++) fccp[i] = s.c_str()[i];
	return fcc;
}

inline uint16_t hexFourcc2short(fourcc_t fcc) {
	return strtol(fourcc2str(fcc).c_str(), nullptr, 16);
}

uint32_t chunkVector::getHeaderSize(bool is_littleEndian) {
	uint32_t totalSize = 0;
	if (is_littleEndian) {
		for (auto& chunk : *this) {
			switch (chunk->id) {
				// case 'SSND':
				// 	totalSize += 16;
				// 	break;

				case 'data':
					totalSize += 8;
					break;

				case 'big1':
					break;

				default:
					totalSize += 8;
					totalSize += chunk->lSize;
					break;
			}
		}
	}
	else {
		for (auto& chunk : *this) {
			switch (chunk->id) {
				case 'SSND':
					totalSize += 16;
					break;

				// case 'data':
				// 	totalSize += 8;
				// 	break;

				case 'big1':
					break;

				default:
					totalSize += 8;
					totalSize += chunk->bSize;
					break;
			}
		}
	}
	return totalSize;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
AudioFile::AudioFile(const char* fPath) : _filePath(filesystem::path(fPath)) {
	//	Do basic checks.
	if (!filesystem::exists(this->_filePath)) throw runtime_error("File not found.");
	if (!filesystem::is_regular_file(this->_filePath)) throw runtime_error("Not a regular file.");

	//	Open file.
	this->fileAccess.open(this->_filePath.string(), ios_base::in | ios_base::out | ios_base::binary);
	if (this->fileAccess.fail())
		throw invalid_argument("There was a problem opening the input file.");

	//	Read start of file to see what it is.
	//	WAVE and AIFF audio files always have a 12 byte header, always the first chunk.
	this->fileAccess.read(reinterpret_cast<char*>(&this->header), 12);

	switch (this->header.id) {
		case 'RIFF':
		case 'RF64':
			if (this->header.type != 'WAVE')
				throw runtime_error("ERROR: Unknown media type.");
			this->baseType = BASE_TYPE_WAVE;
			this->fileSize = this->header.lSize; //	will change if RF64
			break;

		case 'FORM':
			if (this->header.type != 'AIFF' && this->header.type != 'AIFC')
				throw runtime_error("ERROR: Unknown media type.");
			this->baseType = BASE_TYPE_AIFF;
			this->fileSize = this->header.bSize;
			break;

		default:
			throw runtime_error("ERROR: Unknow file type or file type not handled.");
	}

	////////////////////////////////////////////////////////////////////////////////////////////////
	//	Look for Chunks and store appropriately.
	audioFileHeader_t       chunkExam;
	const fstream::pos_type examSize = 8; //	Only load and use the first two fields of chunkExam.
	char*                   chunkPtr;

	do {
		this->fileAccess.read(reinterpret_cast<char*>(&chunkExam), examSize);
		this->fileAccess.seekg(-examSize, ios_base::cur);
		fstream::pos_type wholeChunkSize = (this->is_littleEndian() ? chunkExam.lSize : chunkExam.bSize) + examSize;

		switch (chunkExam.id) {
			case 'data':
				break;

			case 'SSND':
				wholeChunkSize = sizeof(SSND_t); //	id, size, offset, and blockSize; total == 16
			//	fall through
			default:
				chunkPtr = new char[wholeChunkSize];
				this->fileAccess.read(chunkPtr, wholeChunkSize);
				this->chunk.push_back(reinterpret_cast<chunks_t*>(chunkPtr));
				break;
		}

		chunks_t*         dataChunk;
		fstream::pos_type offset;
		switch (chunkExam.id) {
			case 'fmt ':
			case 'COMM':
				this->format = this->chunk.back();
				break;

			case 'fact':
				//	Only set if not already set. Might be set by ds64 chunk.
				if (this->frameCount == -1)
					this->frameCount = this->chunk.back()->fact.samples;
				break;

			case 'ds64': //	will only be in RF64
				this->fileSize = this->chunk.back()->ds64.riffSize;
				this->dataSize   = this->chunk.back()->ds64.dataSize;
				this->frameCount = this->chunk.back()->ds64.sampleCount;
				break;

			case 'data':
				//	WAVE data chunk has not been read.
				dataChunk = reinterpret_cast<chunks_t*>(new char[8]);
				dataChunk->data.id   = chunkExam.id; //	data
				dataChunk->data.size = chunkExam.lSize;
				this->chunk.push_back(reinterpret_cast<chunks_t*>(dataChunk));
				//	Get start point of sound data.
				this->dataStart = this->fileAccess.tellg() + examSize;

				if (this->header.id == 'RIFF') {
					this->dataSize = chunkExam.lSize;
					if (this->frameCount == -1) //	If frameCount not already set by fact chunk.
						this->frameCount = this->dataSize / this->format->fmt_.blockAlignment;
				}
				else {
					//	RF64
					if (this->dataSize == -1)
						throw runtime_error("DataSize not yet set by ds64.\nIs the 'ds64' chunk before 'data' chunk?");
				}

				//	Jump read pointer to end of sound data.
				this->fileAccess.seekg(examSize + this->dataSize, ios_base::cur);
				break;

			case 'SSND':
				if (this->chunk.back()->SSND.blockSize != 0) //	frame size?
					throw runtime_error("Can't handle files with SSND.blockSize set to other than zero.");

				offset          = static_cast<fstream::pos_type>(this->chunk.back()->SSND.offset);
				this->dataStart = this->fileAccess.tellg() + offset;
				//	minus sizeof offset, blockSize, and offset value
				this->dataSize = this->chunk.back()->SSND.size - (8 + offset);

				this->frameCount = this->format->COMM.numSampleFrames;

				//	Jump read pointer to end of sound data.
				//	"minus 8" because we're already pointing past offset and blockSize
				this->fileAccess.seekg(this->chunk.back()->SSND.size - 8, ios_base::cur);
				break;


			default:
				break;
		}
	}
	while (this->fileAccess.good());
}

AudioFile::AudioFile(
		const char*               fPath,
		const uint32_t            sampleRate,
		const uint16_t            sampleSize,
		const uint16_t            numChan,
		const fourcc_t            encoding,
		const baseAudioFileType_t baseType
	) : _filePath(filesystem::path(fPath)), baseType(baseType) {
	if (filesystem::exists(this->_filePath))
		throw runtime_error("File already exists.");

	this->format        = reinterpret_cast<chunks_t*>(new char[sizeof(chunks_t)]);
	this->bytesPerFrame = ceil(sampleSize / 8.0) * numChan;

	switch (this->baseType) {
		case BASE_TYPE_WAVE:
			this->header.id = 'RIFF';
			this->header.lSize                = 0;
			this->header.type                 = 'WAVE';
			this->format->fmt_.id             = 'fmt ';
			this->format->fmt_.size           = sizeof(FormatData_t) - sizeof(waveChunkHead_t);
			this->format->fmt_.type           = hexFourcc2short(encoding);
			this->format->fmt_.channelCount   = numChan;
			this->format->fmt_.sampleRate     = sampleRate;
			this->format->fmt_.bytesPerSecond = sampleRate * this->bytesPerFrame; //	can depend on encoding
			this->format->fmt_.blockAlignment = this->bytesPerFrame;
			this->format->fmt_.bitsPerSample  = sampleSize;
			break;

		case BASE_TYPE_AIFF:
			this->header.id = 'FORM';
			this->header.bSize                 = 0;
			this->header.type                  = 'AIFF';
			this->format->COMM.id              = 'COMM';
			this->format->COMM.numChannels     = numChan;
			this->format->COMM.numSampleFrames = 0;
			this->format->COMM.sampleSize      = sampleSize;
			this->format->COMM.sampleRate      = sampleRate;
			switch (encoding) {
				case 0:
				case '    ':
				case 'PCM ':
				case '0001':
					this->format->COMM.size = sizeof(commChunk_t) - 8;
					break;

				default:
					//	Final size depends on length of COMM.compressionName which has a maximum 256 bytes.
					this->format->COMM.size = sizeof(commExtChunk_t) - 8;
					this->format->COMM.compressionType = encoding;
					//	Pascal, length prefixed string, 0-255.
					//	contains 1 to 256 bytes (must be even)
					//	Last byte should be zero or null
					this->format->COMM.compressionName[0] = 0;
					break;
			}
			break;

		default:
			throw runtime_error("Currently, only WAVE and AIFF files can be created.");
	}
}


AudioFile::~AudioFile() {
	delete this->format;
};


////////////////////////////////////////////////////////////////////////////////////////////////////
string AudioFile::getFileName() const {
	return string(this->_filePath.filename());
}

bool AudioFile::is_pcm() const {
	auto type = this->getDataEncoding();
	if (type == '0001' || type == 'PCM ')
		return true;
	return false;
}

bool AudioFile::is_ieee() const {
	auto type = this->getDataEncoding();
	if (type == '0003' || type == 'fl32')
		return true;
	return false;
};

bool AudioFile::is_littleEndian() const {
	return this->baseType == BASE_TYPE_WAVE;
};

uint32_t AudioFile::getFormatSize() const {
	switch (this->format->id) {
		case 'fmt ':
			return this->format->fmt_.size;

		case'COMM':
			return this->format->COMM.size;
	}
	return 0;
};

uint16_t AudioFile::getNumChannels() const {
	switch (this->format->id) {
		case 'fmt ':
			return static_cast<uint16_t>(this->format->fmt_.channelCount);

		case'COMM':
			return this->format->COMM.numChannels;
	}
	return 0;
};

uint32_t AudioFile::getSampleRate() {
	switch (this->format->id) {
		case 'fmt ':
			return this->format->fmt_.sampleRate;

		case'COMM':
			return static_cast<uint32_t>(this->format->COMM.sampleRate());
	}
	return 0;
};

uint16_t AudioFile::getBitsPerSample() const {
	switch (this->format->id) {
		case 'fmt ':
			return this->format->fmt_.bitsPerSample;

		case'COMM':
			return this->format->COMM.sampleSize;
	}
	return 0;
};

int64_t AudioFile::getNumFrames() const { return this->frameCount; };

int64_t AudioFile::getNumSamples() const { return this->frameCount * this->getNumChannels(); };

int64_t AudioFile::getDataSize() const { return this->dataSize; };

fourcc_t AudioFile::getDataEncoding() const {
	switch (this->format->id) {
		case 'fmt ':
			return short2hexFourcc(this->format->fmt_.type);

		case'COMM':
			//	If > 18 it must be an AIFC file
			if (this->format->COMM.size > 18)
				return this->format->COMM.compressionType;
			else
				return 'PCM ';
	}

	return 0;
};


//	Maximum value storable == 2^(nBits-1) - 1
float64_t AudioFile::getSampleMaxMagnitude() const {
	if (this->is_ieee())
		return 1.0;

	uint16_t bytesPS = ceil(this->getBitsPerSample() / 8.0);
	switch (bytesPS) {
		case 1:
			return numeric_limits<native_int8_t>::max();

		case 2:
			return numeric_limits<native_int16_t>::max();

		case 3:
			return numeric_limits<native_int24_t>::max();

		case 4:
			return numeric_limits<native_int32_t>::max();

		default:
			return numeric_limits<float32_t>::quiet_NaN();
	}
}


////////////////////////////////////////////////////////////////////////////////////////////////////
unsigned char* AudioFile::ReadAllData() {
	if (!filesystem::exists(this->_filePath))
		throw runtime_error("File not found.");

	if (!this->fileAccess.is_open())
		this->fileAccess.open(this->_filePath.string(), ios_base::in | ios_base::out | ios_base::binary);

	auto data = static_cast<unsigned char*>(calloc(this->dataSize, 1));
	this->fileAccess.clear();
	this->fileAccess.seekg(this->dataStart);
	this->fileAccess.read(reinterpret_cast<char*>(data), this->dataSize);
	return data;
}

//	Write new data to existing data block. Other chunks are not changed.
void AudioFile::WriteAllData(const unsigned char* data) {
	if (!filesystem::exists(this->_filePath)) throw runtime_error("File does not yet exist.");

	this->fileAccess.clear();
	this->fileAccess.seekp(this->dataStart);
	this->fileAccess.write(reinterpret_cast<const char*>(data), this->dataSize);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//	Adds pointer to chunk vector and returns index of inserted chunk.
uint16_t AudioFile::addChunk(chunks_t* chk) {
	this->chunk.push_back(chk);
	return this->chunk.size() - 1;
}

//	Returns pointer to chunk at the index.
const chunks_t* AudioFile::getChunk(const uint16_t index) {
	return this->chunk.at(index);
}

//	Returns the total size of all chunks, except just the header of sound data.
size_t AudioFile::getAllChunksSize() {
	size_t allChunksSize = 12;                                           //	file head; id, size, type
	allChunksSize += this->chunk.getHeaderSize(this->is_littleEndian()); //	other chunk sizes, but now audio data proper
	return allChunksSize;
}


void AudioFile::writeUpdatedHeader() {
	//	write all but sound data chunks
	//	Assumes file is already open.
	this->fileAccess.seekp(0, ios_base::beg);
	this->fileAccess.write(reinterpret_cast<const char*>(&this->header), 12);
	for (auto chnk : this->chunk) {
		if (chnk->id != 'data' && chnk->id != 'SSND') {
			streamsize chunkSize = this->is_littleEndian() ? chnk->lSize : chnk->bSize;
			this->fileAccess.write(reinterpret_cast<const char*>(&chnk), chunkSize);
		}
	}

	//	Be sure all is filled up to the start of data/SSND chunk
	//	DataStart minus what we've done, and minus 8 for JUNK/elm1 data header.
	int32_t fill = this->dataStart - this->getAllChunksSize() - 8;
	if (fill < 8)
		throw runtime_error("Header size has increased beyond what was originally created.");

	if (this->is_littleEndian()) {
		waveData_t* JUNK = reinterpret_cast<waveData_t*>(new char[fill]);
		JUNK->id         = 'JUNK';
		JUNK->size       = fill - 8;
		this->fileAccess.write(reinterpret_cast<const char*>(&JUNK), fill);
	}
	else {
		fill -= 8; //	minus 8 more to cover rest of SSND chunk
		aChunk_t* elm1 = reinterpret_cast<aChunk_t*>(new char[fill]);
		elm1->id       = 'elm1';
		elm1->size     = fill - 8;
		this->fileAccess.write(reinterpret_cast<const char*>(&elm1), fill);
	}

	//	Write header of sound data chunk.
	for (auto chnk : this->chunk) {
		switch (chnk->id) {
			case 'data':
				this->fileAccess.write(reinterpret_cast<const char*>(&chnk), 8);
				break;

			case 'SSND':
				this->fileAccess.write(reinterpret_cast<const char*>(&chnk), 16);
				break;

			default:
				break;
		}
	}
	// dataStart should equal seekp here
}



///////////////////////////////////////////////////////////////////////////////////////////////////
void AudioFile::writeNewHeader() {
	//	Create file if not exists with basic metadata.
	//		AIFF generally starts chunk at 512 byte boundary.
	//		WAVE generally starts chunk at 4096 byte boundary.

	if (this->dataStart != 0)
		throw runtime_error("File already has header.");

	if (!this->fileAccess.is_open())
		this->fileAccess.open(this->_filePath.string(), ios_base::in | ios_base::out | ios_base::binary);

	size_t totalSize = this->getAllChunksSize();
	this->dataStart  = this->baseType == BASE_TYPE_WAVE ? 4096 : 512;

	//	increase size to contain all header chunks
	while (totalSize > (this->dataStart - 8)) this->dataStart *= 2;

	this->writeUpdatedHeader();
}


///////////////////////////////////////////////////////////////////////////////////////////////////
// seek relative to start of data
void AudioFile::seekp_data(uint64_t pos, ios_base::seekdir dir) {
	switch (dir) {
		case ios_base::beg:
			this->dataWritePos = pos;
			break;

		case ios_base::cur:
			this->dataWritePos += pos;
			break;

		case ios_base::end:
			this->dataWritePos = this->dataSize - pos;
			break;
	}
	if (this->dataWritePos < 0) this->dataWritePos = 0;

	this->fileAccess.seekp(this->dataWritePos + this->dataStart, ios_base::beg);
}

uint64_t AudioFile::tellp_data() {
	return this->dataWritePos;
}

void AudioFile::write(const char* data, uint64_t size) {
	if (this->dataStart == 0)
		throw runtime_error("Must write header before writing data.");

	if (!this->fileAccess.is_open())
		throw runtime_error("File not open.");

	//	Append size bytes of data to existing data.
	//	Update header to reflect new size
	this->seekp_data(0, ios_base::cur);
	this->fileAccess.write(data, size);
	this->dataSize += size;

	this->dataWritePos = static_cast<int64_t>(this->fileAccess.tellp()) - this->dataStart;

	int64_t fSize = filesystem::file_size(this->_filePath.filename());

	if (this->is_littleEndian())
		this->header.lSize = fSize - 8;
	else
		this->header.bSize = fSize - 8;

	if (this->baseType == BASE_TYPE_WAVE)
		this->format->fmt_.size = this->dataSize - this->dataStart;
	else
		this->format->SSND.size = this->dataSize - this->dataStart;

	this->writeUpdatedHeader();
}

} //  namespace Diskerror
