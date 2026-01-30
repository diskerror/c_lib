//
// Created by Reid Woodbury.
//

#include "AudioFormat.h"
#include "BigFloat80.h"

#include <cstring>
#include <stdexcept>

namespace Diskerror {

using namespace std;


////////////////////////////////////////////////////////////////////////////////
//	fromWaveFmt — parse [ID 4][SIZE 4][payload 16+]
////////////////////////////////////////////////////////////////////////////////

AudioFormat AudioFormat::fromWaveFmt(span<const uint8_t> blob) {
	//	Minimum: 8 header + 16 payload = 24 bytes
	if (blob.size() < 24)
		throw runtime_error("fmt chunk too small.");

	const uint8_t* p = blob.data() + 8; // skip ID and SIZE

	AudioFormat af;
	af.m_sampleLittleEndian = true;

	little_uint16_t formatTag;
	little_uint16_t channels;
	little_uint32_t sampleRate;
	little_uint32_t bytesPerSec;
	little_uint16_t blockAlign;
	little_uint16_t bitsPerSample;

	memcpy(&formatTag,    p,      2);
	memcpy(&channels,     p + 2,  2);
	memcpy(&sampleRate,   p + 4,  4);
	memcpy(&bytesPerSec,  p + 8,  4);
	memcpy(&blockAlign,   p + 12, 2);
	memcpy(&bitsPerSample,p + 14, 2);

	af.m_rawWaveFormatTag = formatTag;
	af.m_channels         = channels;
	af.m_sampleRate       = static_cast<double>(static_cast<uint32_t>(sampleRate));
	af.m_bitsPerSample    = bitsPerSample;

	switch (static_cast<uint16_t>(formatTag)) {
		case 0x0001: af.m_encoding = SampleEncoding::PCM;       break;
		case 0x0003: af.m_encoding = SampleEncoding::Float;     break;
		case 0x0006: af.m_encoding = SampleEncoding::ALaw;      break;
		case 0x0007: af.m_encoding = SampleEncoding::ULaw;      break;
		case 0x0011: af.m_encoding = SampleEncoding::IMA_ADPCM; break;
		case 0xFFFE: af.m_encoding = SampleEncoding::Extensible; break;
		default:     af.m_encoding = SampleEncoding::Other;     break;
	}

	return af;
}


////////////////////////////////////////////////////////////////////////////////
//	fromAiffComm — parse [ID 4][SIZE 4][payload 18+]
////////////////////////////////////////////////////////////////////////////////

AudioFormat AudioFormat::fromAiffComm(span<const uint8_t> blob) {
	//	Minimum: 8 header + 18 payload = 26 bytes
	if (blob.size() < 26)
		throw runtime_error("COMM chunk too small.");

	const uint8_t* p = blob.data() + 8; // skip ID and SIZE

	//	Read payload size from header
	big_uint32_t payloadSizeBE;
	memcpy(&payloadSizeBE, blob.data() + 4, 4);
	uint32_t payloadSize = payloadSizeBE;

	AudioFormat af;
	af.m_sampleLittleEndian = false;

	big_uint16_t channels;
	big_uint32_t numSampleFrames;
	big_uint16_t sampleSize;

	memcpy(&channels,        p,      2);
	memcpy(&numSampleFrames, p + 2,  4);
	memcpy(&sampleSize,      p + 6,  2);

	af.m_channels         = channels;
	af.m_numSampleFrames  = numSampleFrames;
	af.m_bitsPerSample    = sampleSize;

	//	Sample rate is 80-bit extended float at offset 8
	BigFloat80 bf80;
	memcpy(&bf80, p + 8, 10);
	af.m_sampleRate = bf80.toDouble();

	if (payloadSize >= 23) {
		//	Extended COMM (AIFC) — has compressionType + compressionName
		fourcc_t compressionType;
		memcpy(&compressionType, p + 18, 4);
		af.m_rawAifcCompression = compressionType;

		uint32_t ct = compressionType;
		if (ct == 'NONE' || ct == 'twos') {
			af.m_encoding = SampleEncoding::PCM;
			af.m_sampleLittleEndian = false;
		}
		else if (ct == 'sowt') {
			af.m_encoding = SampleEncoding::PCM;
			af.m_sampleLittleEndian = true;
		}
		else if (ct == 'fl32') {
			af.m_encoding = SampleEncoding::Float;
			af.m_bitsPerSample = 32;
		}
		else if (ct == 'fl64') {
			af.m_encoding = SampleEncoding::Float;
			af.m_bitsPerSample = 64;
		}
		else if (ct == 'ulaw') {
			af.m_encoding = SampleEncoding::ULaw;
		}
		else if (ct == 'ALAW') {
			af.m_encoding = SampleEncoding::ALaw;
		}
		else if (ct == 'ima4') {
			af.m_encoding = SampleEncoding::IMA_ADPCM;
		}
		else {
			af.m_encoding = SampleEncoding::Other;
		}
	}
	else {
		//	Plain AIFF — no compression
		af.m_encoding = SampleEncoding::PCM;
		af.m_sampleLittleEndian = false;
	}

	return af;
}


////////////////////////////////////////////////////////////////////////////////
//	toWaveFmt — produce complete 'fmt ' chunk blob
////////////////////////////////////////////////////////////////////////////////

vector<uint8_t> AudioFormat::toWaveFmt() const {
	//	Determine format tag
	uint16_t tag;
	switch (m_encoding) {
		case SampleEncoding::PCM:       tag = 0x0001; break;
		case SampleEncoding::Float:     tag = 0x0003; break;
		case SampleEncoding::ALaw:      tag = 0x0006; break;
		case SampleEncoding::ULaw:      tag = 0x0007; break;
		case SampleEncoding::IMA_ADPCM: tag = 0x0011; break;
		case SampleEncoding::Extensible:tag = 0xFFFE; break;
		case SampleEncoding::Other:     tag = m_rawWaveFormatTag; break;
		default:                        tag = m_rawWaveFormatTag; break;
	}

	//	PCM uses 16-byte payload; non-PCM uses 18 (with cbSize=0)
	uint32_t payloadSize = (tag == 0x0001) ? 16 : 18;
	vector<uint8_t> out(8 + payloadSize);

	//	Chunk ID
	fourcc_t fmtId = 'fmt ';
	memcpy(out.data(), &fmtId, 4);

	//	Chunk size
	little_uint32_t chunkSize = payloadSize;
	memcpy(out.data() + 4, &chunkSize, 4);

	//	Payload
	uint8_t* p = out.data() + 8;

	little_uint16_t leTag          = tag;
	little_uint16_t leChannels     = m_channels;
	little_uint32_t leSampleRate   = static_cast<uint32_t>(m_sampleRate);
	little_uint32_t leBytesPerSec  = bytesPerSecond();
	little_uint16_t leBlockAlign   = bytesPerFrame();
	little_uint16_t leBitsPerSample = m_bitsPerSample;

	memcpy(p,      &leTag,           2);
	memcpy(p + 2,  &leChannels,      2);
	memcpy(p + 4,  &leSampleRate,    4);
	memcpy(p + 8,  &leBytesPerSec,   4);
	memcpy(p + 12, &leBlockAlign,    2);
	memcpy(p + 14, &leBitsPerSample, 2);

	if (payloadSize >= 18) {
		little_uint16_t cbSize = 0;
		memcpy(p + 16, &cbSize, 2);
	}

	return out;
}


////////////////////////////////////////////////////////////////////////////////
//	toAiffComm — produce complete 'COMM' chunk blob
////////////////////////////////////////////////////////////////////////////////

vector<uint8_t> AudioFormat::toAiffComm() const {
	bool aifc = requiresAifc();

	//	Determine compression type and name for AIFC
	fourcc_t compressionType{0};
	string compressionName;

	if (aifc) {
		switch (m_encoding) {
			case SampleEncoding::PCM:
				if (m_sampleLittleEndian) {
					compressionType = 'sowt';
					compressionName = "little-endian";
				}
				else {
					compressionType = 'NONE';
					compressionName = "not compressed";
				}
				break;
			case SampleEncoding::Float:
				if (m_bitsPerSample == 64) {
					compressionType = 'fl64';
					compressionName = "64-bit floating point";
				}
				else {
					compressionType = 'fl32';
					compressionName = "32-bit floating point";
				}
				break;
			case SampleEncoding::ULaw:
				compressionType = 'ulaw';
				compressionName = "u-law";
				break;
			case SampleEncoding::ALaw:
				compressionType = 'ALAW';
				compressionName = "A-law";
				break;
			case SampleEncoding::IMA_ADPCM:
				compressionType = 'ima4';
				compressionName = "IMA ADPCM";
				break;
			case SampleEncoding::Other:
				compressionType = m_rawAifcCompression;
				compressionName = "unknown";
				break;
			default:
				compressionType = m_rawAifcCompression;
				compressionName = "unknown";
				break;
		}
	}

	//	Base payload: 18 bytes (channels 2 + frames 4 + sampleSize 2 + sampleRate 10)
	uint32_t payloadSize = 18;
	if (aifc) {
		//	+ compressionType(4) + Pascal string (1 len + chars)
		uint32_t pstrLen = 1 + static_cast<uint32_t>(compressionName.size());
		//	Pascal string total must be even
		if (pstrLen & 1) pstrLen++;
		payloadSize += 4 + pstrLen;
	}

	vector<uint8_t> out(8 + payloadSize);

	//	Chunk ID
	fourcc_t commId = 'COMM';
	memcpy(out.data(), &commId, 4);

	//	Chunk size (big-endian)
	big_uint32_t chunkSize = payloadSize;
	memcpy(out.data() + 4, &chunkSize, 4);

	//	Payload
	uint8_t* p = out.data() + 8;

	big_uint16_t beChannels     = m_channels;
	big_uint32_t beFrames       = m_numSampleFrames;
	big_uint16_t beSampleSize   = m_bitsPerSample;

	memcpy(p,     &beChannels,   2);
	memcpy(p + 2, &beFrames,     4);
	memcpy(p + 6, &beSampleSize, 2);

	//	Sample rate as 80-bit float
	BigFloat80 bf80(m_sampleRate);
	memcpy(p + 8, &bf80, 10);

	if (aifc) {
		memcpy(p + 18, &compressionType, 4);

		//	Pascal string: length byte + chars + pad to even
		uint8_t nameLen = static_cast<uint8_t>(compressionName.size());
		p[22] = nameLen;
		if (nameLen > 0)
			memcpy(p + 23, compressionName.data(), nameLen);
		//	Pad byte if total (1+nameLen) is odd
		uint32_t pstrTotal = 1 + nameLen;
		if (pstrTotal & 1)
			p[22 + pstrTotal] = 0;
	}

	return out;
}

} // namespace Diskerror
