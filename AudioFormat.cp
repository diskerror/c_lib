//
// Created by Reid Woodbury.
//


#include "AudioFormat.h"
#include "AudioFile.h"

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace Diskerror {

using namespace std;


////////////////////////////////////////////////////////////////////////////////
//	Constructor from AudioFile
////////////////////////////////////////////////////////////////////////////////

AudioFormat::AudioFormat(const AudioFile& file) : m_type(file.type()), m_file(&file) {
	if (m_type == AudioType::Wave) {
		auto idx = file.findChunk('fmt ');
		if (idx.has_value()) {
			auto parsed = fromWaveFmt(file.chunk(*idx));
			m_encoding = parsed.m_encoding;
			m_waveFmt  = parsed.m_waveFmt;
		}
		else {
			//	New file — sensible defaults
			m_encoding = SampleEncoding::PCM;
			m_waveFmt.type           = 0x0001;
			m_waveFmt.channelCount   = 1;
			m_waveFmt.sampleRate     = 44100;
			m_waveFmt.bitsPerSample  = 16;
			m_waveFmt.blockAlignment = 2;      // 1 channel * 2 bytes
			m_waveFmt.bytesPerSecond = 88200;  // 44100 * 2
		}

		//	Read fact chunk if present (non-PCM WAVE stores sample count here)
		auto factIdx = file.findChunk('fact');
		if (factIdx.has_value()) {
			auto blob = file.chunk(*factIdx);
			if (blob.size() >= 12) {
				little_uint32_t sc;
				memcpy(&sc, blob.data() + 8, 4);
				m_factSampleCount = sc;
			}
		}
	}
	else if (m_type == AudioType::Aiff) {
		auto idx = file.findChunk('COMM');
		if (idx.has_value()) {
			auto parsed = fromAiffComm(file.chunk(*idx));
			m_encoding = parsed.m_encoding;
			m_aiffComm = parsed.m_aiffComm;
		}
		else {
			//	New file — sensible defaults
			m_encoding = SampleEncoding::PCM;
			m_aiffComm.numChannels = 1;
			m_aiffComm.sampleSize  = 16;
			m_aiffComm.sampleRate  = 44100.0;
		}
	}
	else {
		throw runtime_error("Cannot create AudioFormat for unknown audio type.");
	}
}


////////////////////////////////////////////////////////////////////////////////
//	fromWaveFmt — parse [ID 4][SIZE 4][payload 16+]
////////////////////////////////////////////////////////////////////////////////

AudioFormat AudioFormat::fromWaveFmt(span<const uint8_t> blob) {
	//	Minimum: 8 header + 16 payload = 24 bytes
	if (blob.size() < 24)
		throw runtime_error("fmt chunk too small.");

	AudioFormat af;
	af.m_type = AudioType::Wave;

	//	Copy blob directly into m_waveFmt (it shares the on-disk layout).
	//	Extra fields beyond blob stay zero-initialized.
	size_t copyLen = min(blob.size(), sizeof(fmt_t));
	memcpy(&af.m_waveFmt, blob.data(), copyLen);

	//	Derive SampleEncoding from the format tag
	switch (static_cast<uint16_t>(af.m_waveFmt.type)) {
		case 0x0001: af.m_encoding = SampleEncoding::PCM;        break;
		case 0x0003: af.m_encoding = SampleEncoding::Float;      break;
		case 0x0006: af.m_encoding = SampleEncoding::ALaw;       break;
		case 0x0007: af.m_encoding = SampleEncoding::ULaw;       break;
		case 0x0011: af.m_encoding = SampleEncoding::IMA_ADPCM;  break;
		case 0xFFFE: af.m_encoding = SampleEncoding::Extensible; break;
		default:     af.m_encoding = SampleEncoding::Other;      break;
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

	AudioFormat af;
	af.m_type = AudioType::Aiff;

	//	Copy the base commChunk_t portion (26 bytes: 8 header + 18 payload).
	//	Cannot memcpy the full COMM_t because compressionName is 256 fixed bytes
	//	but variable-length on disk.
	memcpy(&af.m_aiffComm, blob.data(), 26);

	//	Read payload size to determine if this is extended COMM (AIFC)
	uint32_t payloadSize = af.m_aiffComm.size;

	if (payloadSize >= 23 && blob.size() >= 30) {
		//	AIFC extended COMM: compressionType at offset 26
		memcpy(&af.m_aiffComm.compressionType, blob.data() + 26, 4);

		//	Pascal string at offset 30: length byte + chars
		if (blob.size() > 30) {
			uint8_t nameLen = blob[30];
			size_t available = blob.size() - 31;
			size_t toCopy = min(static_cast<size_t>(nameLen), min(available, size_t(255)));
			af.m_aiffComm.compressionName[0] = static_cast<char>(nameLen);
			if (toCopy > 0)
				memcpy(&af.m_aiffComm.compressionName[1], blob.data() + 31, toCopy);
		}

		//	Derive SampleEncoding from compressionType
		uint32_t ct = af.m_aiffComm.compressionType;
		if (ct == 'NONE' || ct == 'twos') {
			af.m_encoding = SampleEncoding::PCM;
		}
		else if (ct == 'sowt') {
			af.m_encoding = SampleEncoding::PCM;
		}
		else if (ct == 'fl32') {
			af.m_encoding = SampleEncoding::Float;
			af.m_aiffComm.sampleSize = 32;
		}
		else if (ct == 'fl64') {
			af.m_encoding = SampleEncoding::Float;
			af.m_aiffComm.sampleSize = 64;
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
	}

	return af;
}


////////////////////////////////////////////////////////////////////////////////
//	toChunk — produce all format-related chunk blobs
////////////////////////////////////////////////////////////////////////////////

vector<vector<uint8_t>> AudioFormat::toChunk() const {
	vector<vector<uint8_t>> chunks;

	if (m_type == AudioType::Wave) {
		chunks.push_back(toWaveFmt());
		if (m_encoding != SampleEncoding::PCM)
			chunks.push_back(toFactChunk());
	}
	else if (m_type == AudioType::Aiff) {
		if (requiresAifc())
			chunks.push_back(toFverChunk());
		chunks.push_back(toAiffComm());
	}
	else {
		throw runtime_error("Cannot serialize AudioFormat for unknown audio type.");
	}

	return chunks;
}


////////////////////////////////////////////////////////////////////////////////
//	toWaveFmt — produce complete 'fmt ' chunk blob
////////////////////////////////////////////////////////////////////////////////

vector<uint8_t> AudioFormat::toWaveFmt() const {
	//	Determine output size from format tag
	uint16_t tag = m_waveFmt.type;
	size_t totalBytes;

	if (tag == 0x0001) {
		//	PCM: FormatData_t (8 header + 16 payload)
		totalBytes = sizeof(FormatData_t);
	}
	else if (tag == 0xFFFE) {
		//	Extensible: full fmt_t (8 header + 40 payload)
		totalBytes = sizeof(fmt_t);
	}
	else {
		//	Non-PCM, non-Extensible: FormatPlusData_t (8 header + 18 payload)
		totalBytes = sizeof(FormatPlusData_t);
	}

	vector<uint8_t> out(totalBytes);
	memcpy(out.data(), &m_waveFmt, totalBytes);

	//	Ensure chunk ID and size are correct
	fourcc_t fmtId = 'fmt ';
	memcpy(out.data(), &fmtId, 4);

	little_uint32_t payloadSize = static_cast<uint32_t>(totalBytes - 8);
	memcpy(out.data() + 4, &payloadSize, 4);

	return out;
}


////////////////////////////////////////////////////////////////////////////////
//	toAiffComm — produce complete 'COMM' chunk blob
////////////////////////////////////////////////////////////////////////////////

vector<uint8_t> AudioFormat::toAiffComm() const {
	bool aifc = requiresAifc();

	if (!aifc) {
		//	Plain AIFF: 26 bytes (8 header + 18 payload)
		vector<uint8_t> out(26);
		memcpy(out.data(), &m_aiffComm, 26);

		//	Ensure chunk header
		fourcc_t commId = 'COMM';
		memcpy(out.data(), &commId, 4);
		big_uint32_t payloadSize = 18;
		memcpy(out.data() + 4, &payloadSize, 4);

		return out;
	}

	//	AIFC: base 26 bytes + 4 compressionType + Pascal string
	uint8_t nameLen = static_cast<uint8_t>(m_aiffComm.compressionName[0]);
	uint32_t pstrTotal = 1 + nameLen;
	if (pstrTotal & 1) pstrTotal++; // pad to even

	uint32_t payloadSize = 18 + 4 + pstrTotal;
	vector<uint8_t> out(8 + payloadSize);

	//	Copy base 26 bytes (header + base payload)
	memcpy(out.data(), &m_aiffComm, 26);

	//	Ensure chunk header
	fourcc_t commId = 'COMM';
	memcpy(out.data(), &commId, 4);
	big_uint32_t bPayloadSize = payloadSize;
	memcpy(out.data() + 4, &bPayloadSize, 4);

	//	compressionType at offset 26
	memcpy(out.data() + 26, &m_aiffComm.compressionType, 4);

	//	Pascal string at offset 30
	out[30] = nameLen;
	if (nameLen > 0)
		memcpy(out.data() + 31, &m_aiffComm.compressionName[1], nameLen);
	//	Pad byte is already zero from vector initialization

	return out;
}


////////////////////////////////////////////////////////////////////////////////
//	toFactChunk — produce 'fact' chunk for non-PCM WAVE
////////////////////////////////////////////////////////////////////////////////

vector<uint8_t> AudioFormat::toFactChunk() const {
	vector<uint8_t> out(12);

	fourcc_t factId = 'fact';
	memcpy(out.data(), &factId, 4);

	little_uint32_t payloadSize = 4;
	memcpy(out.data() + 4, &payloadSize, 4);

	little_uint32_t sampleCount = numSampleFrames();
	memcpy(out.data() + 8, &sampleCount, 4);

	return out;
}


////////////////////////////////////////////////////////////////////////////////
//	toFverChunk — produce 'FVER' chunk for AIFC
////////////////////////////////////////////////////////////////////////////////

vector<uint8_t> AudioFormat::toFverChunk() const {
	vector<uint8_t> out(12);

	fourcc_t fverId = 'FVER';
	memcpy(out.data(), &fverId, 4);

	big_uint32_t payloadSize = 4;
	memcpy(out.data() + 4, &payloadSize, 4);

	big_uint32_t timestamp = 0xA2805140;  // AIFF-C spec creation date
	memcpy(out.data() + 8, &timestamp, 4);

	return out;
}


////////////////////////////////////////////////////////////////////////////////
//	requiresAifc
////////////////////////////////////////////////////////////////////////////////

bool AudioFormat::requiresAifc() const {
	return m_encoding != SampleEncoding::PCM;
}


////////////////////////////////////////////////////////////////////////////////
//	setEncoding — sync struct fields with encoding
////////////////////////////////////////////////////////////////////////////////

void AudioFormat::setEncoding(SampleEncoding e) {
	m_encoding = e;

	if (m_type == AudioType::Wave) {
		switch (e) {
			case SampleEncoding::PCM:        m_waveFmt.type = 0x0001; break;
			case SampleEncoding::Float:      m_waveFmt.type = 0x0003; break;
			case SampleEncoding::ALaw:       m_waveFmt.type = 0x0006; break;
			case SampleEncoding::ULaw:       m_waveFmt.type = 0x0007; break;
			case SampleEncoding::IMA_ADPCM:  m_waveFmt.type = 0x0011; break;
			case SampleEncoding::Extensible: m_waveFmt.type = 0xFFFE; break;
			default: break;
		}
	}
	else if (m_type == AudioType::Aiff) {
		switch (e) {
			case SampleEncoding::PCM:
				m_aiffComm.compressionType = 'NONE';
				break;
			case SampleEncoding::Float:
				m_aiffComm.compressionType = (bitsPerSample() == 64) ? 'fl64' : 'fl32';
				break;
			case SampleEncoding::ULaw:
				m_aiffComm.compressionType = 'ulaw';
				break;
			case SampleEncoding::ALaw:
				m_aiffComm.compressionType = 'ALAW';
				break;
			case SampleEncoding::IMA_ADPCM:
				m_aiffComm.compressionType = 'ima4';
				break;
			default: break;
		}
	}
}


////////////////////////////////////////////////////////////////////////////////
//	Convenience accessors — dispatch to appropriate struct
////////////////////////////////////////////////////////////////////////////////

uint16_t AudioFormat::channels() const {
	if (m_type == AudioType::Wave)
		return m_waveFmt.channelCount;
	else
		return m_aiffComm.numChannels;
}

double AudioFormat::sampleRate() const {
	if (m_type == AudioType::Wave)
		return static_cast<double>(static_cast<uint32_t>(m_waveFmt.sampleRate));
	else
		return m_aiffComm.sampleRate.toDouble();
}

uint16_t AudioFormat::bitsPerSample() const {
	if (m_type == AudioType::Wave)
		return m_waveFmt.bitsPerSample;
	else
		return m_aiffComm.sampleSize;
}

uint32_t AudioFormat::numSampleFrames() const {
	if (m_type == AudioType::Aiff)
		return m_aiffComm.numSampleFrames;
	//	WAVE: use fact chunk sample count if available (compressed formats)
	if (m_factSampleCount > 0)
		return m_factSampleCount;
	//	WAVE: calculate from dataSize for linear formats
	if (m_file && bytesPerFrame() > 0)
		return static_cast<uint32_t>(m_file->dataSize() / bytesPerFrame());
	return 0;
}

uint16_t AudioFormat::bytesPerFrame() const {
	return channels() * ((bitsPerSample() + 7) / 8);
}

uint32_t AudioFormat::bytesPerSecond() const {
	return bytesPerFrame() * static_cast<uint32_t>(sampleRate());
}


////////////////////////////////////////////////////////////////////////////////
//	Convenience setters
////////////////////////////////////////////////////////////////////////////////

void AudioFormat::setChannels(uint16_t c) {
	if (m_type == AudioType::Wave) {
		m_waveFmt.channelCount = c;
		//	Update derived WAVE fields
		m_waveFmt.blockAlignment = c * ((static_cast<uint16_t>(m_waveFmt.bitsPerSample) + 7) / 8);
		m_waveFmt.bytesPerSecond = static_cast<uint32_t>(m_waveFmt.blockAlignment)
		                         * static_cast<uint32_t>(m_waveFmt.sampleRate);
	}
	else {
		m_aiffComm.numChannels = c;
	}
}

void AudioFormat::setSampleRate(double r) {
	if (m_type == AudioType::Wave) {
		m_waveFmt.sampleRate = static_cast<uint32_t>(r);
		m_waveFmt.bytesPerSecond = static_cast<uint32_t>(m_waveFmt.blockAlignment)
		                         * static_cast<uint32_t>(m_waveFmt.sampleRate);
	}
	else {
		m_aiffComm.sampleRate = r;
	}
}

void AudioFormat::setBitsPerSample(uint16_t b) {
	if (m_type == AudioType::Wave) {
		m_waveFmt.bitsPerSample = b;
		uint16_t ch = m_waveFmt.channelCount;
		m_waveFmt.blockAlignment = ch * ((b + 7) / 8);
		m_waveFmt.bytesPerSecond = static_cast<uint32_t>(m_waveFmt.blockAlignment)
		                         * static_cast<uint32_t>(m_waveFmt.sampleRate);
	}
	else {
		m_aiffComm.sampleSize = b;
	}
}

void AudioFormat::setNumSampleFrames(uint32_t n) {
	if (m_type == AudioType::Aiff)
		m_aiffComm.numSampleFrames = n;
	else if (m_type == AudioType::Wave)
		m_factSampleCount = n;
}


////////////////////////////////////////////////////////////////////////////////
//	updateFrameCount — auto-compute from data size for linear formats
////////////////////////////////////////////////////////////////////////////////

void AudioFormat::updateFrameCount(int64_t dataSize) {
	uint16_t bpf = bytesPerFrame();
	if (isLinear() && bpf > 0) {
		uint32_t frames = static_cast<uint32_t>(dataSize / bpf);
		setNumSampleFrames(frames);
	}
}

} // namespace Diskerror
