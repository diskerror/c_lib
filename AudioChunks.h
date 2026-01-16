//
// Created by Reid Woodbury.
//

#ifndef DISKERROR_AUDIOCHUNKS_H
#define DISKERROR_AUDIOCHUNKS_H

#include <vector>
#include <memory>
#include <cstdint>
#include <cstring>
#include <string>
#include <boost/endian/conversion.hpp>
#include "WAVE.h"
#include "AIFF.h"
#include "BigFloat80.h"

namespace Diskerror {

using namespace boost::endian;

/**
 * Base class for all RIFF/AIFF chunks.
 */
class Chunk {
public:
    fourcc_t id;
    
    explicit Chunk(fourcc_t id) : id(id) {}
    virtual ~Chunk() = default;

    /**
     * Returns the serialized bytes of the chunk (Header + Data).
     * NOTE: For DataChunk, this ONLY returns the header (ID + Size + SSND-offsets).
     * The actual audio data is not returned here to prevent massive memory allocations.
     */
    virtual std::vector<uint8_t> serialize(bool isLittleEndian) const = 0;
    
    /**
     * Returns the total size in bytes of the chunk (Header + Data + PaddingByte).
     */
    virtual uint32_t totalSize() const = 0;
};

/**
 * For chunks we don't need to parse but must preserve.
 */
class UnknownChunk : public Chunk {
public:
    std::vector<uint8_t> payload;

    UnknownChunk(fourcc_t id, const std::vector<uint8_t>& data) 
        : Chunk(id), payload(data) {}

    std::vector<uint8_t> serialize(bool isLittleEndian) const override {
        std::vector<uint8_t> out(8 + payload.size());
        // ID is always big-endian FourCC
        memcpy(out.data(), &id, 4);
        
        uint32_t sz = static_cast<uint32_t>(payload.size());
        if (isLittleEndian) {
            native_to_little_inplace(sz);
        } else {
            native_to_big_inplace(sz);
        }
        memcpy(out.data() + 4, &sz, 4);
        
        if (!payload.empty()) {
            memcpy(out.data() + 8, payload.data(), payload.size());
        }
        return out;
    }

    uint32_t totalSize() const override { return 8 + payload.size(); }
};

/**
 * Alignment padding chunk (JUNK for WAVE, PAD for AIFF).
 * Used to align the subsequent chunk to a specific boundary (e.g., 4k).
 */
class AlignmentChunk : public Chunk {
public:
    uint32_t payloadSize; // The size of the padding payload (excluding header)

    AlignmentChunk(fourcc_t id, uint32_t sizeBytes) : Chunk(id), payloadSize(sizeBytes) {}

    std::vector<uint8_t> serialize(bool isLittleEndian) const override {
        std::vector<uint8_t> out(8 + payloadSize); // Header + Zeros
        memcpy(out.data(), &id, 4);
        
        uint32_t sz = payloadSize;
        if (isLittleEndian) {
            native_to_little_inplace(sz);
        } else {
            native_to_big_inplace(sz);
        }
        memcpy(out.data() + 4, &sz, 4);
        
        // Zero fill payload
        if (payloadSize > 0) {
            std::memset(out.data() + 8, 0, payloadSize);
        }
        return out;
    }

    uint32_t totalSize() const override { return 8 + payloadSize; }
};

/**
 * Broadcast Audio Extension ('bext') chunk.
 */
class BextChunk : public Chunk {
public:
    char            description[256] = {0};
    char            originator[32] = {0};
    char            originatorReference[32] = {0};
    char            originationDate[10] = {0}; // yyyy:mm:dd
    char            originationTime[8] = {0};  // hh:mm:ss
    int64_t         timeReference = 0;
    uint16_t        version = 0;
    uint8_t         umid[64] = {0};
    int16_t         loudnessValue = 0;
    uint16_t        loudnessRange = 0;
    int16_t         maxTruePeakLevel = 0;
    int16_t         maxMomentaryLoudness = 0;
    int16_t         maxShortTermLoudness = 0;
    std::vector<uint8_t> codingHistory;

    BextChunk() : Chunk('bext') {}

    std::vector<uint8_t> serialize(bool isLittleEndian) const override {
        uint32_t payloadSize = 602 + static_cast<uint32_t>(codingHistory.size());
        std::vector<uint8_t> out(8 + payloadSize);
        
        memcpy(out.data(), &id, 4);
        uint32_t sz = payloadSize;
        if (isLittleEndian) native_to_little_inplace(sz);
        else native_to_big_inplace(sz);
        memcpy(out.data() + 4, &sz, 4);

        uint8_t* p = out.data() + 8;
        memcpy(p, description, 256); p += 256;
        memcpy(p, originator, 32); p += 32;
        memcpy(p, originatorReference, 32); p += 32;
        memcpy(p, originationDate, 10); p += 10;
        memcpy(p, originationTime, 8); p += 8;

        int64_t tr = timeReference;
        uint16_t ver = version;
        int16_t lv = loudnessValue;
        uint16_t lr = loudnessRange;
        int16_t mtp = maxTruePeakLevel;
        int16_t mml = maxMomentaryLoudness;
        int16_t mst = maxShortTermLoudness;

        if (isLittleEndian) {
            native_to_little_inplace(tr);
            native_to_little_inplace(ver);
            native_to_little_inplace(lv);
            native_to_little_inplace(lr);
            native_to_little_inplace(mtp);
            native_to_little_inplace(mml);
            native_to_little_inplace(mst);
        }

        memcpy(p, &tr, 8); p += 8;
        memcpy(p, &ver, 2); p += 2;
        memcpy(p, umid, 64); p += 64;
        memcpy(p, &lv, 2); p += 2;
        memcpy(p, &lr, 2); p += 2;
        memcpy(p, &mtp, 2); p += 2;
        memcpy(p, &mml, 2); p += 2;
        memcpy(p, &mst, 2); p += 2;
        
        // Reserved (180 bytes)
        memset(p, 0, 180); p += 180;

        if (!codingHistory.empty()) {
            memcpy(p, codingHistory.data(), codingHistory.size());
        }

        return out;
    }

    uint32_t totalSize() const override { return 8 + 602 + codingHistory.size(); }
};

/**
 * Data Size 64 ('ds64') chunk for RF64.
 */
class Ds64Chunk : public Chunk {
public:
    uint64_t riffSize = 0;
    uint64_t dataSize = 0;
    uint64_t sampleCount = 0;
    // We don't support the table for now, keep it simple

    Ds64Chunk() : Chunk('ds64') {}

    std::vector<uint8_t> serialize(bool isLittleEndian) const override {
        std::vector<uint8_t> out(8 + 28);
        memcpy(out.data(), &id, 4);
        
        uint32_t sz = 28;
        if (isLittleEndian) native_to_little_inplace(sz);
        else native_to_big_inplace(sz);
        memcpy(out.data() + 4, &sz, 4);

        uint64_t rs = riffSize;
        uint64_t ds = dataSize;
        uint64_t sc = sampleCount;
        uint32_t tl = 0;

        if (isLittleEndian) {
            native_to_little_inplace(rs);
            native_to_little_inplace(ds);
            native_to_little_inplace(sc);
            native_to_little_inplace(tl);
        }

        memcpy(out.data() + 8, &rs, 8);
        memcpy(out.data() + 16, &ds, 8);
        memcpy(out.data() + 24, &sc, 8);
        memcpy(out.data() + 32, &tl, 4);

        return out;
    }

    uint32_t totalSize() const override { return 8 + 28; }
};

/**
 * Fact ('fact') chunk.
 */
class FactChunk : public Chunk {
public:
    uint32_t sampleCount = 0;

    FactChunk() : Chunk('fact') {}

    std::vector<uint8_t> serialize(bool isLittleEndian) const override {
        std::vector<uint8_t> out(12);
        memcpy(out.data(), &id, 4);
        
        uint32_t sz = 4;
        uint32_t sc = sampleCount;
        if (isLittleEndian) {
            native_to_little_inplace(sz);
            native_to_little_inplace(sc);
        } else {
            native_to_big_inplace(sz);
            native_to_big_inplace(sc);
        }
        memcpy(out.data() + 4, &sz, 4);
        memcpy(out.data() + 8, &sc, 4);
        return out;
    }

    uint32_t totalSize() const override { return 12; }
};

/**
 * Special chunk for audio data. 
 * serialize() returns ONLY the header.
 */
class DataChunk : public Chunk {
public:
    uint32_t payloadSize = 0;
    uint64_t fileOffset = 0; // Where it is in the source file
    std::vector<uint8_t> memoryBuffer; // If we want to write new data from memory
    
    // AIFF SSND specific fields
    uint32_t offset = 0;
    uint32_t blockSize = 0;

    explicit DataChunk(fourcc_t id) : Chunk(id) {}

    std::vector<uint8_t> serialize(bool isLittleEndian) const override {
        uint32_t totalPayloadSize = payloadSize;
        bool isSSND = (id == 'SSND');
        
        if (isSSND) {
            totalPayloadSize += 8; // account for offset and blockSize fields
        }
        
        std::vector<uint8_t> out(8 + (isSSND ? 8 : 0));
        
        // ID
        memcpy(out.data(), &id, 4);
        
        // Size
        uint32_t sz = totalPayloadSize;
        if (isLittleEndian) native_to_little_inplace(sz);
        else native_to_big_inplace(sz);
        memcpy(out.data() + 4, &sz, 4);

        // SSND extra fields
        if (isSSND) {
            uint32_t off = offset;
            uint32_t blk = blockSize;
            native_to_big_inplace(off);
            native_to_big_inplace(blk);
            memcpy(out.data() + 8, &off, 4);
            memcpy(out.data() + 12, &blk, 4);
        }
        
        return out;
    }

    uint32_t totalSize() const override {
        uint32_t sz = 8 + payloadSize;
        if (id == 'SSND') sz += 8;
        return sz;
    }
};

/**
 * WAVE 'fmt ' chunk.
 */
class WaveFmtChunk : public Chunk {
public:
    uint16_t formatTag      = 1; // PCM
    uint16_t numChannels    = 1;
    uint32_t sampleRate     = 44100;
    uint32_t bytesPerSec    = 88200;
    uint16_t blockAlign     = 2;
    uint16_t bitsPerSample  = 16;
    std::vector<uint8_t> extension;

    WaveFmtChunk() : Chunk('fmt ') {}

    std::vector<uint8_t> serialize(bool isLittleEndian) const override {
        uint32_t payloadSize = 16 + (extension.empty() ? 0 : 2 + extension.size());
        std::vector<uint8_t> out(8 + payloadSize);
        
        memcpy(out.data(), &id, 4);
        
        uint32_t sz = payloadSize;
        if (isLittleEndian) native_to_little_inplace(sz);
        else native_to_big_inplace(sz); // Should likely not happen for WAVE
        memcpy(out.data() + 4, &sz, 4);
        
        uint16_t tag = formatTag; 
        uint16_t chan = numChannels; 
        uint32_t rate = sampleRate; 
        uint32_t bps = bytesPerSec; 
        uint16_t align = blockAlign; 
        uint16_t bits = bitsPerSample; 
        
        if (isLittleEndian) {
            native_to_little_inplace(tag);
            native_to_little_inplace(chan);
            native_to_little_inplace(rate);
            native_to_little_inplace(bps);
            native_to_little_inplace(align);
            native_to_little_inplace(bits);
        } else {
             native_to_big_inplace(tag);
             native_to_big_inplace(chan);
             native_to_big_inplace(rate);
             native_to_big_inplace(bps);
             native_to_big_inplace(align);
             native_to_big_inplace(bits);
        }
        
        memcpy(out.data() + 8, &tag, 2);
        memcpy(out.data() + 10, &chan, 2);
        memcpy(out.data() + 12, &rate, 4);
        memcpy(out.data() + 16, &bps, 4);
        memcpy(out.data() + 20, &align, 2);
        memcpy(out.data() + 22, &bits, 2);
        
        if (!extension.empty()) {
            uint16_t cbSize = static_cast<uint16_t>(extension.size());
            if (isLittleEndian) native_to_little_inplace(cbSize);
            else native_to_big_inplace(cbSize);
            
            memcpy(out.data() + 24, &cbSize, 2);
            memcpy(out.data() + 26, extension.data(), extension.size());
        }
        
        return out;
    }

    uint32_t totalSize() const override { 
        return 8 + 16 + (extension.empty() ? 0 : 2 + extension.size()); 
    }
};

/**
 * AIFF 'COMM' chunk.
 */
class AiffCommChunk : public Chunk {
public:
    uint16_t numChannels     = 1;
    uint32_t numSampleFrames = 0;
    uint16_t sampleSize      = 16;
    double sampleRate   = 44100.0;
    fourcc_t compressionType = 0;
    std::string compressionName;

    AiffCommChunk() : Chunk('COMM') {}

    std::vector<uint8_t> serialize(bool /*isLittleEndian*/) const override {
        // AIFF is always Big Endian, ignoring isLittleEndian param effectively
        
        uint32_t payloadSize = 18;
        if (compressionType != 0) {
            payloadSize += 4 + 1 + compressionName.size();
            if (payloadSize % 2 != 0) payloadSize++; 
        }
        
        std::vector<uint8_t> out(8 + payloadSize);
        memcpy(out.data(), &id, 4);
        
        uint32_t sz = payloadSize;
        native_to_big_inplace(sz);
        memcpy(out.data() + 4, &sz, 4);
        
        uint16_t chan = numChannels; native_to_big_inplace(chan);
        uint32_t frames = numSampleFrames; native_to_big_inplace(frames);
        uint16_t bits = sampleSize; native_to_big_inplace(bits);
        
        memcpy(out.data() + 8, &chan, 2);
        memcpy(out.data() + 10, &frames, 4);
        memcpy(out.data() + 14, &bits, 2);
        
        // Sample Rate (80-bit float)
        BigFloat80 bf80(sampleRate);
        memcpy(out.data() + 16, &bf80, 10);
        
        if (compressionType != 0) {
            // compressionType is FourCC, usually already BE if treated as char array
            memcpy(out.data() + 26, &compressionType, 4);
            
            uint8_t nameLen = static_cast<uint8_t>(compressionName.size());
            out[30] = nameLen;
            memcpy(out.data() + 31, compressionName.data(), nameLen);
        }
        
        return out;
    }

    uint32_t totalSize() const override { 
        uint32_t sz = 18;
        if (compressionType != 0) {
            sz += 4 + 1 + compressionName.size();
            if (sz % 2 != 0) sz++;
        }
        return 8 + sz;
    }
};

} // namespace Diskerror

#endif // DISKERROR_AUDIOCHUNKS_H