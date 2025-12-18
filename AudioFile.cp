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

namespace Diskerror {

using namespace std;
using namespace boost;
using namespace boost::endian;

//  Convert a four-char-code (big endian 32-bit integer) to a string of 4 characters.
inline string fourcc2str(fourcc_t fcc) {
	return string(reinterpret_cast<const char*>(&fcc), 4);
}

inline fourcc_t short2hexFourcc(const uint16_t in) {
	return *reinterpret_cast<fourcc_t*>(format("{:04X}", in).data());
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
AudioFile::AudioFile(const char* fPath) : the_file_path(filesystem::path(fPath)) {
	//	Do basic checks.
	if (!filesystem::exists(this->the_file_path)) throw runtime_error("File not found.");
	if (!filesystem::is_regular_file(this->the_file_path)) throw runtime_error("Not a regular file.");

	//	Open file.
	this->file_access.open(this->the_file_path.string(), ios_base::in | ios_base::out | ios_base::binary);
	if (this->file_access.fail())
		throw invalid_argument("There was a problem opening the input file.");

	//	Read start of file to see what it is.
	//	WAVE and AIFF audio files always have a 12 byte header, always the first chunk.
	this->file_access.read(reinterpret_cast<char*>(&this->header), 12);

	switch (this->header.id) {
		case 'RIFF':
		case 'RF64':
			if (this->header.type != 'WAVE')
				throw runtime_error("ERROR: Unknown media type.");
			this->base_type = BASE_TYPE_WAVE;
			this->file_size = this->header.lSize; //	will change if RF64
			break;

		case 'FORM':
			if (this->header.type != 'AIFF' && this->header.type != 'AIFC')
				throw runtime_error("ERROR: Unknown media type.");
			this->base_type = BASE_TYPE_AIFF;
			this->file_size = this->header.bSize;
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
		this->file_access.read(reinterpret_cast<char*>(&chunkExam), examSize);
		this->file_access.seekg(-examSize, ios_base::cur);
		fstream::pos_type wholeChunkSize = (this->is_littleEndian() ? chunkExam.lSize : chunkExam.bSize) + examSize;

		switch (chunkExam.id) {
			case 'data':
				break;

			case 'SSND':
				wholeChunkSize = sizeof(SSND_t); //	id, size, offset, and blockSize; total == 16
			//	fall through
			default:
				chunkPtr = new char[wholeChunkSize];
				this->file_access.read(chunkPtr, wholeChunkSize);
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
				if (this->frame_count == -1)
					this->frame_count = this->chunk.back()->fact.samples;
				break;

			case 'ds64': //	will only be in RF64
				this->file_size = this->chunk.back()->ds64.riffSize;
				this->data_size   = this->chunk.back()->ds64.dataSize;
				this->frame_count = this->chunk.back()->ds64.sampleCount;
				break;

			case 'data':
				//	WAVE data chunk has not been read.
				dataChunk = reinterpret_cast<chunks_t*>(new char[8]);
				dataChunk->data.id   = chunkExam.id; //	data
				dataChunk->data.size = chunkExam.lSize;
				this->chunk.push_back(reinterpret_cast<chunks_t*>(dataChunk));
				//	Get start point of sound data.
				this->data_start = this->file_access.tellg() + examSize;

				if (this->header.id == 'RIFF') {
					this->data_size = chunkExam.lSize;
					if (this->frame_count == -1) //	If frame_count not already set by fact chunk.
						this->frame_count = this->data_size / this->format->fmt_.blockAlignment;
				}
				else {
					//	RF64
					if (this->data_size == -1)
						throw runtime_error("DataSize not yet set by ds64.\nIs the 'ds64' chunk before 'data' chunk?");
				}

				//	Jump read pointer to end of sound data.
				this->file_access.seekg(examSize + this->data_size, ios_base::cur);
				break;

			case 'SSND':
				if (this->chunk.back()->SSND.blockSize != 0) //	frame size?
					throw runtime_error("Can't handle files with SSND.blockSize set to other than zero.");

				offset           = static_cast<fstream::pos_type>(this->chunk.back()->SSND.offset);
				this->data_start = this->file_access.tellg() + offset;
				//	minus sizeof offset, blockSize, and offset value
				this->data_size = this->chunk.back()->SSND.size - (8 + offset);

				this->frame_count = this->format->COMM.numSampleFrames;

				//	Jump read pointer to end of sound data.
				//	"minus 8" because we're already pointing past offset and blockSize
				this->file_access.seekg(this->chunk.back()->SSND.size - 8, ios_base::cur);
				break;


			default:
				break;
		}
	}
	while (this->file_access.good());
}

AudioFile::AudioFile(
		const char*               fPath,
		const uint32_t            sampleRate,
		const uint16_t            sampleSize,
		const uint16_t            numChan,
		const fourcc_t            encoding,
		const baseAudioFileType_t baseType
	) : the_file_path(filesystem::path(fPath)), base_type(baseType) {
	if (filesystem::exists(this->the_file_path))
		throw runtime_error("File already exists.");

	this->format          = reinterpret_cast<chunks_t*>(new char[sizeof(chunks_t)]);
	this->bytes_per_frame = ceil(sampleSize / 8.0) * numChan;

	switch (this->base_type) {
		case BASE_TYPE_WAVE:
			this->header.id = 'RIFF';
			this->header.lSize                = 0;
			this->header.type                 = 'WAVE';
			this->format->fmt_.id             = 'fmt ';
			this->format->fmt_.size           = sizeof(FormatData_t) - sizeof(waveChunkHead_t);
			this->format->fmt_.type           = hexFourcc2short(encoding);
			this->format->fmt_.channelCount   = numChan;
			this->format->fmt_.sampleRate     = sampleRate;
			this->format->fmt_.bytesPerSecond = sampleRate * this->bytes_per_frame; //	can depend on encoding
			this->format->fmt_.blockAlignment = this->bytes_per_frame;
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
	return string(this->the_file_path.filename());
}

baseAudioFileType_t AudioFile::getBaseType() const {
	return this->base_type;
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
	return this->base_type == BASE_TYPE_WAVE;
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

int64_t AudioFile::getNumFrames() const { return this->frame_count; };

int64_t AudioFile::getNumSamples() const { return this->frame_count * this->getNumChannels(); };

int64_t AudioFile::getDataSize() const { return this->data_size; };

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


////////////////////////////////////////////////////////////////////////////////////////////////////
unsigned char* AudioFile::ReadAllData() {
	if (!filesystem::exists(this->the_file_path))
		throw runtime_error("File not found.");

	if (!this->file_access.is_open())
		this->file_access.open(this->the_file_path.string(), ios_base::in | ios_base::out | ios_base::binary);

	auto data = static_cast<unsigned char*>(calloc(this->data_size, 1));
	this->file_access.clear();
	this->file_access.seekg(this->data_start);
	this->file_access.read(reinterpret_cast<char*>(data), this->data_size);
	return data;
}

//	Write new data to existing data block. Other chunks are not changed.
void AudioFile::WriteAllData(const unsigned char* data) {
	if (!filesystem::exists(this->the_file_path)) throw runtime_error("File does not yet exist.");

	this->file_access.clear();
	this->file_access.seekp(this->data_start);
	this->file_access.write(reinterpret_cast<const char*>(data), this->data_size);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//	Adds pointer to chunk vector and returns index of inserted chunk.
uint16_t AudioFile::addChunk(chunks_t* chk) {
	this->chunk.push_back(chk);
	return this->chunk.size() - 1;
}

uint16_t AudioFile::getChunkCount() const {
	return this->chunk.size();
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
	this->file_access.seekp(0, ios_base::beg);
	this->file_access.write(reinterpret_cast<const char*>(&this->header), 12);
	for (auto chnk : this->chunk) {
		if (chnk->id != 'data' && chnk->id != 'SSND') {
			streamsize chunkSize = this->is_littleEndian() ? chnk->lSize : chnk->bSize;
			this->file_access.write(reinterpret_cast<const char*>(&chnk), chunkSize);
		}
	}

	//	Be sure all is filled up to the start of data/SSND chunk
	//	DataStart minus what we've done, and minus 8 for JUNK/elm1 data header.
	int32_t fill = this->data_start - this->getAllChunksSize() - 8;
	if (fill < 8)
		throw runtime_error("Header size has increased beyond what was originally created.");

	if (this->is_littleEndian()) {
		waveData_t* JUNK = reinterpret_cast<waveData_t*>(new char[fill]);
		JUNK->id         = 'JUNK';
		JUNK->size       = fill - 8;
		this->file_access.write(reinterpret_cast<const char*>(&JUNK), fill);
	}
	else {
		fill -= 8; //	minus 8 more to cover rest of SSND chunk
		aChunk_t* elm1 = reinterpret_cast<aChunk_t*>(new char[fill]);
		elm1->id       = 'elm1';
		elm1->size     = fill - 8;
		this->file_access.write(reinterpret_cast<const char*>(&elm1), fill);
	}

	//	Write header of sound data chunk.
	for (auto chnk : this->chunk) {
		switch (chnk->id) {
			case 'data':
				this->file_access.write(reinterpret_cast<const char*>(&chnk), 8);
				break;

			case 'SSND':
				this->file_access.write(reinterpret_cast<const char*>(&chnk), 16);
				break;

			default:
				break;
		}
	}
	// data_start should equal seekp here
}



///////////////////////////////////////////////////////////////////////////////////////////////////
void AudioFile::writeNewHeader() {
	//	Create file if not exists with basic metadata.
	//		AIFF generally starts chunk at 512 byte boundary.
	//		WAVE generally starts chunk at 4096 byte boundary.

	if (this->data_start != 0)
		throw runtime_error("File already has header.");

	if (!this->file_access.is_open())
		this->file_access.open(this->the_file_path.string(), ios_base::in | ios_base::out | ios_base::binary);

	size_t totalSize = this->getAllChunksSize();
	this->data_start = this->base_type == BASE_TYPE_WAVE ? 4096 : 512;

	//	increase size to contain all header chunks
	while (totalSize > (this->data_start - 8)) this->data_start *= 2;

	this->writeUpdatedHeader();
}


///////////////////////////////////////////////////////////////////////////////////////////////////
// seek relative to start of data
void AudioFile::seekp_data(uint64_t pos, ios_base::seekdir dir) {
	switch (dir) {
		case ios_base::beg:
			this->data_write_pos = pos;
			break;

		case ios_base::cur:
			this->data_write_pos += pos;
			break;

		case ios_base::end:
			this->data_write_pos = this->data_size - pos;
			break;
	}
	if (this->data_write_pos < 0) this->data_write_pos = 0;

	this->file_access.seekp(this->data_write_pos + this->data_start, ios_base::beg);
}

uint64_t AudioFile::tellp_data() {
	return this->data_write_pos;
}

void AudioFile::write(const char* data, uint64_t size) {
	if (this->data_start == 0)
		throw runtime_error("Must write header before writing data.");

	if (!this->file_access.is_open())
		throw runtime_error("File not open.");

	//	Append size bytes of data to existing data.
	//	Update header to reflect new size
	this->seekp_data(0, ios_base::cur);
	this->file_access.write(data, size);
	this->data_size += size;

	this->data_write_pos = static_cast<int64_t>(this->file_access.tellp()) - this->data_start;

	int64_t fSize = filesystem::file_size(this->the_file_path.filename());

	if (this->is_littleEndian())
		this->header.lSize = fSize - 8;
	else
		this->header.bSize = fSize - 8;

	if (this->base_type == BASE_TYPE_WAVE)
		this->format->fmt_.size = this->data_size - this->data_start;
	else
		this->format->SSND.size = this->data_size - this->data_start;

	this->writeUpdatedHeader();
}

} //  namespace Diskerror
