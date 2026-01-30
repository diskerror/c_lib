//
// Simple round-trip tests for AudioFormat + AudioFile.
// Creates WAVE, AIFF, and AIFC files, then re-opens and verifies.
//

#include "AudioFile.h"
#include "AudioFormat.h"

#include <cmath>
#include <cstring>
#include <filesystem>
#include <iostream>

#include <boost/endian/arithmetic.hpp>

using namespace Diskerror;
using namespace std;
using namespace boost::endian;

static int passed = 0;
static int failed = 0;

#define CHECK(cond, msg) do { \
	if (!(cond)) { \
		cerr << "  FAIL: " << msg << endl; \
		failed++; \
	} else { \
		passed++; \
	} \
} while(0)

static const int    NUM_FRAMES  = 8;
static const double SAMPLE_RATE = 44100.0;

//	16-bit PCM test pattern (quarter-period sine-ish)
static const int16_t pcm16[NUM_FRAMES] = {
	0, 8192, 16384, 8192, 0, -8192, -16384, -8192
};

//	32-bit float test pattern
static const float fl32[NUM_FRAMES] = {
	0.0f, 0.25f, 0.5f, 0.25f, 0.0f, -0.25f, -0.5f, -0.25f
};


////////////////////////////////////////////////////////////////////////////////
//	WAVE  PCM 16-bit  44100 Hz  mono
////////////////////////////////////////////////////////////////////////////////

static void testWavePCM16(const filesystem::path& dir) {
	cout << "--- WAVE PCM 16-bit ---" << endl;
	const auto path = dir / "test_wave_pcm16.wav";

	//	Remove leftover from previous run
	if (filesystem::exists(path))
		filesystem::remove(path);

	//	Create and write
	{
		AudioFile af(path, AudioType::Wave);
		auto& fmt = af.format();
		fmt.setEncoding(SampleEncoding::PCM);
		fmt.setChannels(1);
		fmt.setSampleRate(SAMPLE_RATE);
		fmt.setBitsPerSample(16);

		af.flush();

		//	Write little-endian int16 samples
		little_int16_t buf[NUM_FRAMES];
		for (int i = 0; i < NUM_FRAMES; i++)
			buf[i] = pcm16[i];

		af.write(reinterpret_cast<const char*>(buf), sizeof(buf));
		af.flush();
	}

	//	Re-open and verify
	{
		AudioFile af(path);

		CHECK(af.type() == AudioType::Wave,                       "type == Wave");
		CHECK(af.format().encoding() == SampleEncoding::PCM,      "encoding == PCM");
		CHECK(af.format().channels() == 1,                        "channels == 1");
		CHECK(af.format().bitsPerSample() == 16,                  "bitsPerSample == 16");
		CHECK(af.format().sampleRate() == SAMPLE_RATE,            "sampleRate == 44100");
		CHECK(af.format().sampleLittleEndian() == true,           "sampleLittleEndian");
		CHECK(af.dataSize() == NUM_FRAMES * 2,                    "dataSize");

		little_int16_t buf[NUM_FRAMES];
		auto n = af.read(reinterpret_cast<char*>(buf), sizeof(buf));
		CHECK(n == static_cast<streamsize>(sizeof(buf)),          "read byte count");

		bool samplesOk = true;
		for (int i = 0; i < NUM_FRAMES; i++) {
			if (static_cast<int16_t>(buf[i]) != pcm16[i]) {
				cerr << "  sample[" << i << "]: expected " << pcm16[i]
				     << ", got " << static_cast<int16_t>(buf[i]) << endl;
				samplesOk = false;
			}
		}
		CHECK(samplesOk, "all samples match");
	}

	cout << "  file: " << path << endl;
}


////////////////////////////////////////////////////////////////////////////////
//	AIFF  PCM 16-bit  44100 Hz  mono
////////////////////////////////////////////////////////////////////////////////

static void testAiffPCM16(const filesystem::path& dir) {
	cout << "--- AIFF PCM 16-bit ---" << endl;
	const auto path = dir / "test_aiff_pcm16.aiff";

	if (filesystem::exists(path))
		filesystem::remove(path);

	//	Create and write
	{
		AudioFile af(path, AudioType::Aiff);
		auto& fmt = af.format();
		fmt.setEncoding(SampleEncoding::PCM);
		fmt.setChannels(1);
		fmt.setSampleRate(SAMPLE_RATE);
		fmt.setBitsPerSample(16);
		//	sampleLittleEndian defaults to false — correct for AIFF

		af.flush();

		//	Write big-endian int16 samples
		big_int16_t buf[NUM_FRAMES];
		for (int i = 0; i < NUM_FRAMES; i++)
			buf[i] = pcm16[i];

		af.write(reinterpret_cast<const char*>(buf), sizeof(buf));
		af.flush();
	}

	//	Re-open and verify
	{
		AudioFile af(path);

		CHECK(af.type() == AudioType::Aiff,                       "type == Aiff");
		CHECK(af.format().encoding() == SampleEncoding::PCM,      "encoding == PCM");
		CHECK(af.format().channels() == 1,                        "channels == 1");
		CHECK(af.format().bitsPerSample() == 16,                  "bitsPerSample == 16");
		CHECK(af.format().sampleRate() == SAMPLE_RATE,            "sampleRate == 44100");
		CHECK(af.format().sampleLittleEndian() == false,          "sampleLittleEndian == false");
		CHECK(af.dataSize() == NUM_FRAMES * 2,                    "dataSize");

		big_int16_t buf[NUM_FRAMES];
		auto n = af.read(reinterpret_cast<char*>(buf), sizeof(buf));
		CHECK(n == static_cast<streamsize>(sizeof(buf)),          "read byte count");

		bool samplesOk = true;
		for (int i = 0; i < NUM_FRAMES; i++) {
			if (static_cast<int16_t>(buf[i]) != pcm16[i]) {
				cerr << "  sample[" << i << "]: expected " << pcm16[i]
				     << ", got " << static_cast<int16_t>(buf[i]) << endl;
				samplesOk = false;
			}
		}
		CHECK(samplesOk, "all samples match");
	}

	cout << "  file: " << path << endl;
}


////////////////////////////////////////////////////////////////////////////////
//	AIFC  Float32  44100 Hz  mono
////////////////////////////////////////////////////////////////////////////////

static void testAifcFloat32(const filesystem::path& dir) {
	cout << "--- AIFC Float32 ---" << endl;
	const auto path = dir / "test_aifc_fl32.aiff";

	if (filesystem::exists(path))
		filesystem::remove(path);

	//	Create and write
	{
		AudioFile af(path, AudioType::Aiff);
		auto& fmt = af.format();
		fmt.setEncoding(SampleEncoding::Float);
		fmt.setChannels(1);
		fmt.setSampleRate(SAMPLE_RATE);
		fmt.setBitsPerSample(32);
		//	sampleLittleEndian defaults to false — correct for AIFC fl32

		af.flush();

		//	Write big-endian float32 samples
		//	boost::endian doesn't have float types, so convert via uint32_t
		uint8_t buf[NUM_FRAMES * 4];
		for (int i = 0; i < NUM_FRAMES; i++) {
			uint32_t bits;
			memcpy(&bits, &fl32[i], 4);
			big_uint32_t be = bits;
			memcpy(buf + i * 4, &be, 4);
		}

		af.write(reinterpret_cast<const char*>(buf), sizeof(buf));
		af.flush();
	}

	//	Re-open and verify
	{
		AudioFile af(path);

		CHECK(af.type() == AudioType::Aiff,                       "type == Aiff");
		CHECK(af.format().encoding() == SampleEncoding::Float,    "encoding == Float");
		CHECK(af.format().channels() == 1,                        "channels == 1");
		CHECK(af.format().bitsPerSample() == 32,                  "bitsPerSample == 32");
		CHECK(af.format().sampleRate() == SAMPLE_RATE,            "sampleRate == 44100");
		CHECK(af.format().sampleLittleEndian() == false,          "sampleLittleEndian == false");
		CHECK(af.format().requiresAifc() == true,                 "requiresAifc");
		CHECK(af.dataSize() == NUM_FRAMES * 4,                    "dataSize");

		uint8_t buf[NUM_FRAMES * 4];
		auto n = af.read(reinterpret_cast<char*>(buf), sizeof(buf));
		CHECK(n == static_cast<streamsize>(sizeof(buf)),          "read byte count");

		bool samplesOk = true;
		for (int i = 0; i < NUM_FRAMES; i++) {
			big_uint32_t be;
			memcpy(&be, buf + i * 4, 4);
			uint32_t bits = be;
			float val;
			memcpy(&val, &bits, 4);
			if (val != fl32[i]) {
				cerr << "  sample[" << i << "]: expected " << fl32[i]
				     << ", got " << val << endl;
				samplesOk = false;
			}
		}
		CHECK(samplesOk, "all samples match");
	}

	cout << "  file: " << path << endl;
}


////////////////////////////////////////////////////////////////////////////////

int main() {
	const filesystem::path outDir = "test_output";
	filesystem::create_directories(outDir);

	testWavePCM16(outDir);
	testAiffPCM16(outDir);
	testAifcFloat32(outDir);

	cout << endl;
	cout << "Results: " << passed << " passed, " << failed << " failed" << endl;

	return failed > 0 ? 1 : 0;
}
