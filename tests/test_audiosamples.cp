//
// Simple tests for AudioSamples read/write with different formats.
// Tests WAVE PCM, AIFF PCM, and AIFC Float using AudioSamples API.
//

#include "../AudioFile.h"
#include "../AudioFormat.h"
#include "../AudioSamples.h"

#include <cmath>
#include <filesystem>
#include <iostream>

using namespace Diskerror;
using namespace std;

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

// Expected normalized float values for PCM 16-bit test pattern
// Original: 0, 8192, 16384, 8192, 0, -8192, -16384, -8192
// Normalized by 32768.0
static const float pcm16_normalized[NUM_FRAMES] = {
    0.0f, 0.25f, 0.5f, 0.25f, 0.0f, -0.25f, -0.5f, -0.25f
};

// Expected float values for float32 test pattern (already normalized)
static const float fl32[NUM_FRAMES] = {
    0.0f, 0.25f, 0.5f, 0.25f, 0.0f, -0.25f, -0.5f, -0.25f
};

// Tolerance for float comparison
static const float EPSILON = 1e-5f;

static bool floatEqual(float a, float b) {
    return std::fabs(a - b) < EPSILON;
}


////////////////////////////////////////////////////////////////////////////////
//  Test reading WAVE PCM 16-bit via AudioSamples
////////////////////////////////////////////////////////////////////////////////

static void testReadWavePCM16(const filesystem::path& dir) {
    cout << "--- Read WAVE PCM 16-bit via AudioSamples ---" << endl;
    const auto path = dir / "test_wave_pcm16.wav";

    if (!filesystem::exists(path)) {
        cerr << "  SKIP: test file not found: " << path << endl;
        return;
    }

    AudioFile af(path);
    AudioFormat fmt(af);
    AudioSamples samples(&af, &fmt);

    CHECK(fmt.encoding() == SampleEncoding::PCM,   "encoding == PCM");
    CHECK(fmt.bitsPerSample() == 16,               "bitsPerSample == 16");
    CHECK(fmt.channels() == 1,                     "channels == 1");

    AudioBuffer buf = samples.readAll();

    CHECK(buf.size() == 1,                         "buffer has 1 channel");
    CHECK(buf[0].size() == NUM_FRAMES,             "buffer has 8 frames");

    bool samplesOk = true;
    for (int i = 0; i < NUM_FRAMES; i++) {
        if (!floatEqual(buf[0][i], pcm16_normalized[i])) {
            cerr << "  sample[" << i << "]: expected " << pcm16_normalized[i]
                 << ", got " << buf[0][i] << endl;
            samplesOk = false;
        }
    }
    CHECK(samplesOk, "all samples normalized correctly");

    cout << "  file: " << path << endl;
}


////////////////////////////////////////////////////////////////////////////////
//  Test reading AIFF PCM 16-bit via AudioSamples
////////////////////////////////////////////////////////////////////////////////

static void testReadAiffPCM16(const filesystem::path& dir) {
    cout << "--- Read AIFF PCM 16-bit via AudioSamples ---" << endl;
    const auto path = dir / "test_aiff_pcm16.aiff";

    if (!filesystem::exists(path)) {
        cerr << "  SKIP: test file not found: " << path << endl;
        return;
    }

    AudioFile af(path);
    AudioFormat fmt(af);
    AudioSamples samples(&af, &fmt);

    CHECK(fmt.encoding() == SampleEncoding::PCM,   "encoding == PCM");
    CHECK(fmt.bitsPerSample() == 16,               "bitsPerSample == 16");
    CHECK(fmt.channels() == 1,                     "channels == 1");
    CHECK(af.isLittleEndian() == false,            "bigEndian (AIFF)");

    AudioBuffer buf = samples.readAll();

    CHECK(buf.size() == 1,                         "buffer has 1 channel");
    CHECK(buf[0].size() == NUM_FRAMES,             "buffer has 8 frames");

    bool samplesOk = true;
    for (int i = 0; i < NUM_FRAMES; i++) {
        if (!floatEqual(buf[0][i], pcm16_normalized[i])) {
            cerr << "  sample[" << i << "]: expected " << pcm16_normalized[i]
                 << ", got " << buf[0][i] << endl;
            samplesOk = false;
        }
    }
    CHECK(samplesOk, "all samples normalized correctly");

    cout << "  file: " << path << endl;
}


////////////////////////////////////////////////////////////////////////////////
//  Test reading AIFC Float32 via AudioSamples
////////////////////////////////////////////////////////////////////////////////

static void testReadAifcFloat32(const filesystem::path& dir) {
    cout << "--- Read AIFC Float32 via AudioSamples ---" << endl;
    const auto path = dir / "test_aifc_fl32.aiff";

    if (!filesystem::exists(path)) {
        cerr << "  SKIP: test file not found: " << path << endl;
        return;
    }

    AudioFile af(path);
    AudioFormat fmt(af);
    AudioSamples samples(&af, &fmt);

    CHECK(fmt.encoding() == SampleEncoding::Float, "encoding == Float");
    CHECK(fmt.bitsPerSample() == 32,               "bitsPerSample == 32");
    CHECK(fmt.channels() == 1,                     "channels == 1");

    AudioBuffer buf = samples.readAll();

    CHECK(buf.size() == 1,                         "buffer has 1 channel");
    CHECK(buf[0].size() == NUM_FRAMES,             "buffer has 8 frames");

    bool samplesOk = true;
    for (int i = 0; i < NUM_FRAMES; i++) {
        if (!floatEqual(buf[0][i], fl32[i])) {
            cerr << "  sample[" << i << "]: expected " << fl32[i]
                 << ", got " << buf[0][i] << endl;
            samplesOk = false;
        }
    }
    CHECK(samplesOk, "all float samples read correctly");

    cout << "  file: " << path << endl;
}


////////////////////////////////////////////////////////////////////////////////
//  Test write/read round-trip for WAVE PCM 16-bit
////////////////////////////////////////////////////////////////////////////////

static void testRoundTripWavePCM16(const filesystem::path& dir) {
    cout << "--- Round-trip WAVE PCM 16-bit via AudioSamples ---" << endl;
    const auto path = dir / "test_audiosamples_wave_pcm16.wav";

    if (filesystem::exists(path))
        filesystem::remove(path);

    // Create test buffer with known values
    AudioBuffer writeBuf(1);  // mono
    writeBuf[0].resize(NUM_FRAMES);
    for (int i = 0; i < NUM_FRAMES; i++) {
        writeBuf[0][i] = pcm16_normalized[i];
    }

    // Create file and write
    {
        AudioFile af(path, AudioType::Wave);
        AudioFormat fmt(af);
        fmt.setEncoding(SampleEncoding::PCM);
        fmt.setChannels(1);
        fmt.setSampleRate(SAMPLE_RATE);
        fmt.setBitsPerSample(16);

        for (auto& blob : fmt.toChunk())
            af.setChunk(std::move(blob));
        af.flush();

        AudioSamples samples(&af, &fmt);
        samples.writeAll(writeBuf, false);  // no dither for exact comparison

        fmt.updateFrameCount(af.dataSize());
        for (auto& blob : fmt.toChunk())
            af.setChunk(std::move(blob));
        af.flush();
    }

    // Re-open and read back
    {
        AudioFile af(path);
        AudioFormat fmt(af);
        AudioSamples samples(&af, &fmt);

        AudioBuffer readBuf = samples.readAll();

        CHECK(readBuf.size() == 1,             "read buffer has 1 channel");
        CHECK(readBuf[0].size() == NUM_FRAMES, "read buffer has 8 frames");

        bool samplesOk = true;
        for (int i = 0; i < NUM_FRAMES; i++) {
            if (!floatEqual(readBuf[0][i], writeBuf[0][i])) {
                cerr << "  sample[" << i << "]: wrote " << writeBuf[0][i]
                     << ", read " << readBuf[0][i] << endl;
                samplesOk = false;
            }
        }
        CHECK(samplesOk, "round-trip samples match");
    }

    cout << "  file: " << path << endl;
}


////////////////////////////////////////////////////////////////////////////////
//  Test write/read round-trip for WAVE Float32
////////////////////////////////////////////////////////////////////////////////

static void testRoundTripWaveFloat32(const filesystem::path& dir) {
    cout << "--- Round-trip WAVE Float32 via AudioSamples ---" << endl;
    const auto path = dir / "test_audiosamples_wave_fl32.wav";

    if (filesystem::exists(path))
        filesystem::remove(path);

    // Create test buffer
    AudioBuffer writeBuf(1);  // mono
    writeBuf[0].resize(NUM_FRAMES);
    for (int i = 0; i < NUM_FRAMES; i++) {
        writeBuf[0][i] = fl32[i];
    }

    // Create file and write
    {
        AudioFile af(path, AudioType::Wave);
        AudioFormat fmt(af);
        fmt.setEncoding(SampleEncoding::Float);
        fmt.setChannels(1);
        fmt.setSampleRate(SAMPLE_RATE);
        fmt.setBitsPerSample(32);

        for (auto& blob : fmt.toChunk())
            af.setChunk(std::move(blob));
        af.flush();

        AudioSamples samples(&af, &fmt);
        samples.writeAll(writeBuf);

        fmt.updateFrameCount(af.dataSize());
        for (auto& blob : fmt.toChunk())
            af.setChunk(std::move(blob));
        af.flush();
    }

    // Re-open and read back
    {
        AudioFile af(path);
        AudioFormat fmt(af);
        AudioSamples samples(&af, &fmt);

        CHECK(fmt.encoding() == SampleEncoding::Float, "encoding == Float");

        AudioBuffer readBuf = samples.readAll();

        CHECK(readBuf.size() == 1,             "read buffer has 1 channel");
        CHECK(readBuf[0].size() == NUM_FRAMES, "read buffer has 8 frames");

        bool samplesOk = true;
        for (int i = 0; i < NUM_FRAMES; i++) {
            if (!floatEqual(readBuf[0][i], writeBuf[0][i])) {
                cerr << "  sample[" << i << "]: wrote " << writeBuf[0][i]
                     << ", read " << readBuf[0][i] << endl;
                samplesOk = false;
            }
        }
        CHECK(samplesOk, "round-trip float samples match");
    }

    cout << "  file: " << path << endl;
}


////////////////////////////////////////////////////////////////////////////////
//  Test readRange with partial reads
////////////////////////////////////////////////////////////////////////////////

static void testReadRange(const filesystem::path& dir) {
    cout << "--- Test readRange partial reads ---" << endl;
    const auto path = dir / "test_wave_pcm16.wav";

    if (!filesystem::exists(path)) {
        cerr << "  SKIP: test file not found: " << path << endl;
        return;
    }

    AudioFile af(path);
    AudioFormat fmt(af);
    AudioSamples samples(&af, &fmt);

    // Read frames 2-5 (4 frames)
    AudioBuffer buf = samples.readRange(2, 4);

    CHECK(buf.size() == 1,     "buffer has 1 channel");
    CHECK(buf[0].size() == 4,  "buffer has 4 frames");

    // Expected: indices 2,3,4,5 of pcm16_normalized
    // 0.5, 0.25, 0.0, -0.25
    CHECK(floatEqual(buf[0][0], 0.5f),   "frame 0 == 0.5");
    CHECK(floatEqual(buf[0][1], 0.25f),  "frame 1 == 0.25");
    CHECK(floatEqual(buf[0][2], 0.0f),   "frame 2 == 0.0");
    CHECK(floatEqual(buf[0][3], -0.25f), "frame 3 == -0.25");

    cout << "  readRange(2, 4) OK" << endl;
}


////////////////////////////////////////////////////////////////////////////////
//  Test normalize static method
////////////////////////////////////////////////////////////////////////////////

static void testNormalize() {
    cout << "--- Test AudioSamples::normalize ---" << endl;

    AudioBuffer buf(2);  // stereo
    buf[0] = {0.1f, 0.2f, 0.3f, 0.4f};
    buf[1] = {-0.1f, -0.2f, -0.3f, -0.5f};  // max magnitude is 0.5

    AudioSamples::normalize(buf, 1.0f);

    // After normalization, max magnitude should be 1.0
    // Scale factor: 1.0 / 0.5 = 2.0
    CHECK(floatEqual(buf[0][0], 0.2f),  "ch0[0] scaled to 0.2");
    CHECK(floatEqual(buf[0][3], 0.8f),  "ch0[3] scaled to 0.8");
    CHECK(floatEqual(buf[1][3], -1.0f), "ch1[3] scaled to -1.0 (peak)");

    cout << "  normalize to 1.0 OK" << endl;
}


////////////////////////////////////////////////////////////////////////////////

int main() {
    const filesystem::path testDir = "tests";

    // Read tests (using files created by test_audioformat)
    testReadWavePCM16(testDir);
    testReadAiffPCM16(testDir);
    testReadAifcFloat32(testDir);

    // Round-trip tests
    testRoundTripWavePCM16(testDir);
    testRoundTripWaveFloat32(testDir);

    // Partial read test
    testReadRange(testDir);

    // Normalize test
    testNormalize();

    cout << endl;
    cout << "Results: " << passed << " passed, " << failed << " failed" << endl;

    return failed > 0 ? 1 : 0;
}