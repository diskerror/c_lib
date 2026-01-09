//
// Created by Reid Woodbury.
//

#ifndef DISKERROR_AUDIOFILE_H
#define DISKERROR_AUDIOFILE_H


#include <filesystem>
#include <fstream>
#include <vector>
#include <span>
#include <memory>

#include <boost/cstdfloat.hpp>
#include <boost/endian/arithmetic.hpp>

#include "AIFF.h"
#include "WAVE.h"

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
	BASE_TYPE_WAVX, //	RIFX: WAVE, big-endian
	BASE_TYPE_8SVX, //	FORM: 8SVX, Amiga IFF 8-bit, big-endian
	BASE_TYPE_MAUD  //	FORM: MAUD, Amiga IFF multi-channel, big-endian
} baseAudioFileType_t;

typedef struct {
	fourcc_t id;
	//	RIFF or RF64: WAVE, little-endian (and BWF)
	//	FORM: AIFF or AIFC, big-endian (except 'swot')
	union {
		little_uint32_t lSize;
		big_uint32_t    bSize;
	};

	fourcc_t type;
} audioFileHeader_t;

typedef union {
	struct {
		fourcc_t id;

		union {
			little_uint32_t lSize;
			big_uint32_t    bSize;
		};

		char rawData[];
	};

	fmt_t  fmt_; //	WAVE format
	COMM_t COMM; //	AIFF format

//	additional AIFF chunks
	//	minf.size==16?
	SSND_t     SSND;
	aChunk_t   elm1; //	elm1.size==426, filler like JUNK?
	MARK_t     MARK;
	INST_t     INST;
	aiffData_t MIDI;
	aiffData_t AESD;
	APPL_t     APPL;
	COMT_t     COMT;
	text_t     NAME;
	text_t     AUTH;
	text_t     copy;
	text_t     ANNO;
	SAXL_t     SAXL;

//	additional WAVE chunks
	waveData_t data;
	ds64_t     ds64;
	waveData_t JUNK;
	bext_t     bext;
	big1_t     big1;
	fact_t     fact;
	cue_t      cue_;
	plst_t     plst;
	listHead_t list;
	smpl_t     smpl;
	inst_t     inst;
} chunks_t;

// Custom deleter for chunks_t which are allocated as char arrays
struct ChunkDeleter {
	void operator()(chunks_t* p) const {
		delete[] reinterpret_cast<char*>(p);
	}
};

//	Defined in AudioFile.cp
class chunkVector : public vector<unique_ptr<chunks_t, ChunkDeleter>> {
public:
	uint32_t getHeaderSize(bool is_littleEndian);
};

/**
 *	class AudioFile
 */
class AudioFile {
	const filesystem::path the_file_path;

	baseAudioFileType_t base_type = BASE_TYPE_UNKNOWN;
	fstream             file_access;
	audioFileHeader_t   header = {0, {0}, 0};
	chunkVector         chunk;
	chunks_t*           format; // Non-owning pointer (observer)

	int64_t  file_size{-1}; // size of FORM or RF64 block (file size - 8)
	uint32_t data_start{0}; // first byte of sound data in file
	int64_t  data_size{-1}; // size of data chunk
	int16_t  bytes_per_frame{-1};
	int64_t  frame_count{-1}; // sample count of fact chunk (frames)

	int64_t data_write_pos = 0;

	size_t getAllChunksSize();

public:
	// Constructors
	//	Use for opening an existing file.
	//	Throws error if file does not exist.
	explicit AudioFile(const filesystem::path&);

	//	Use for creating a new file.
	//	Throws error if file already exists.
	explicit AudioFile(
			const filesystem::path& fPath,
			uint32_t            sampleRate,
			uint16_t            sampleSize,
			uint16_t            numChan,
			big_uint32_t        encoding,
			baseAudioFileType_t baseType
		);

	//	Copy
	AudioFile(const AudioFile& f) : the_file_path(f.the_file_path) {
		throw runtime_error("Copy constructor not allowed.");
	};

	// Destructor
	~AudioFile();

	string              getFileName() const;
	baseAudioFileType_t getBaseType() const;

	[[nodiscard]] bool is_pcm() const;
	[[nodiscard]] bool is_ieee() const;
	[[nodiscard]] bool is_littleEndian() const;

	[[nodiscard]] uint32_t getFormatSize() const;
	[[nodiscard]] uint16_t getNumChannels() const;
	uint32_t               getSampleRate();
	[[nodiscard]] uint16_t getBitsPerSample() const;
	[[nodiscard]] int64_t  getNumFrames() const;
	[[nodiscard]] int64_t  getNumSamples() const;
	[[nodiscard]] int64_t  getDataSize() const;
	[[nodiscard]] fourcc_t getDataEncoding() const;

	vector<unsigned char> ReadAllData();
	void                  WriteAllData(span<const unsigned char>);

	uint16_t        addChunk(chunks_t*);
	uint16_t        getChunkCount() const;
	const chunks_t* getChunk(uint16_t);

	void writeUpdatedHeader();
	void writeNewHeader();

	void     seekp_data(uint64_t, ios_base::seekdir);
	uint64_t tellp_data();
	void     write(const char*, uint64_t);
};

} // namespace Diskerror

#endif // DISKERROR_AUDIOFILE_H
