/*
 * Created by Reid Woodbury Jr on 11/11/25.
 *
 * https://software.intel.com/en-us/articles/floating-point-reference-sheet-for-intel-architecture
 *  (v2.13)
 */

#ifndef DISKERROR_BIGEXT80_H
#define DISKERROR_BIGEXT80_H

#include <cmath>
#include <boost/cstdfloat.hpp>
#include <boost/endian/arithmetic.hpp>

namespace Diskerror {

using namespace boost;
using namespace boost::endian;
using namespace std;


/**
 * @class BigFloat80
 * @brief Represents a floating-point number with extended 80-bit precision.
 *
 * This class provides read and write functionality for reading older,
 *   non-standard big-endian 80-bit floating point numbers. It will not
 *   assist with 80-bit calculations.
 * This class was originally designed to read and write an AIFF audio file's
 *   FORM chunk sampleRate value.
 */
class BigFloat80 {
	big_uint16_t sign_exponent = 0;
	big_uint64_t mantissa      = 0;

public:
	//	Create self.
	BigFloat80() = default;

	//	Create self and convert from float.
	BigFloat80(float64_t);

	//	Create self and convert from integer.
	BigFloat80(int64_t);
	BigFloat80(int);

	//	Copy from unknown 10 byte block of memory, byte by byte. Dangerous.
	explicit BigFloat80(const char*);

	//	Copy-constructor.
	BigFloat80(const BigFloat80&) = default;

	//	Destructor.
	~BigFloat80() = default;


	[[nodiscard]]
	float64_t toDouble() const; //	can cause loss of precision or overvalue E: 15->11, M:63->52
	void            fromDouble(float64_t);


	BigFloat80& operator=(float64_t);       //	Assign a native endian float to this.
	BigFloat80& operator=(uint32_t);        //	Assign an integer to this.
	explicit    operator float64_t() const; //	Assign this to native endian float.
	float64_t   operator()() const;         //	Return float.
};

typedef BigFloat80 big_float80_t;


}

#endif // DISKERROR_BIGEXT80_H
