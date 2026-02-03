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

} // namespace Diskerror

#endif // DISKERROR_AUDIOTYPES_H
