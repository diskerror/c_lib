//
// Shared audio type definitions.
//

#ifndef DISKERROR_AUDIOTYPES_H
#define DISKERROR_AUDIOTYPES_H
#pragma once

#include <cstdint>

#include <boost/endian/arithmetic.hpp>
using namespace boost::endian;

//	Four-character code.
typedef big_uint32_t fourcc_t;

namespace Diskerror {

enum class AudioType : uint8_t {
	Unknown = 0,
	Wave,
	Aiff
};

//	Generic chunk header: [ID 4][SIZE 4]
//	SIZE is little-endian for WAVE, big-endian for AIFF.
//	Use lSize for WAVE files, bSize for AIFF files.
struct ChunkHead {
	fourcc_t id{0};
	union {
		little_uint32_t lSize;
		big_uint32_t    bSize;
	};
};

} // namespace Diskerror

#endif // DISKERROR_AUDIOTYPES_H
