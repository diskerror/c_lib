//
// Created by Reid Woodbury Jr on 11/11/25.
//

#include "BigFloat80.h"

#include <limits>
#include <boost/endian/arithmetic.hpp>

namespace Diskerror {

using namespace boost;
using namespace boost::endian;
using namespace std;

typedef union {
	big_float64_t f64;
	big_uint64_t  i64;
	big_uint16_t  i16; //	first 2 bytes
} big_fi_t;

BigFloat80::BigFloat80(const float64_t input) { this->fromDouble(input); }
BigFloat80::BigFloat80(const int64_t input) { this->fromDouble(static_cast<float64_t>(input)); }
BigFloat80::BigFloat80(const int input) { this->fromDouble(input); }

BigFloat80::BigFloat80(const char* inPtr) {
	auto thisPtr = reinterpret_cast<char*>(this);
	for (uint16_t i = 0; i < 10; i++) { thisPtr[i] = inPtr[i]; }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
const float64_t BigFloat80::toDouble() const {
	big_fi_t fi;

	//	If j-bit is "on" and exponent is "zero" then is it a denormalized value of "1.0"?
	//	For now, change it to a normal value of 1.0.
	if (this->mantissa == 0x8000000000000000 && this->sign_exponent == 0) {
		// fi.i64 = 0x3FF0000000000000;
		// return static_cast<float64_t>(fi.f64);
		return 1.0;
	}

	fi.i64            = (this->mantissa & 0x7FFFFFFFFFFFFFFF) >> 11; //	ignore j-bit
	big_uint16_t expo = (this->sign_exponent & 0x7FFF) - 16383 + 1023;
	fi.i16            |= (expo << 4 & 0x7FF0) | (this->sign_exponent & 0x8000);

	return fi.f64;


	// const float32_t    sign     = this->sign_exponent & 0x8000 ? -1.0 : 1.0;
	// const big_uint16_t exponent = this->sign_exponent & 0x7FFF;

	//  unusual number, infinity or NaN,
	// if (exponent == 0x7FFF) {
	// 	//  infinity, check all 64 bits
	// 	if ((this->mantissa & 0x7FFFFFFFFFFFFFFF) == 0) {
	// 		return sign<0 ? -numeric_limits<float64_t>::infinity() : numeric_limits<float64_t>::infinity();
	// 	}
	//
	// 	// highest mantissa bit set means quiet NaN
	// 	return (this->mantissa & 0x8000000000000000) ?
	// 			   numeric_limits<float64_t>::quiet_NaN() :
	// 			   numeric_limits<float64_t>::signaling_NaN();
	// }
	//
	// // 1 bit sign, 15 bit exponent, 64 bit mantissa
	// // value = (-1) ^ s * (normalizeBit + m / 2 ^ 63) * 2 ^ (e - 16383)
	// return sign * this->mantissa * powl(2.0L, (exponent - 16383) - 63);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void BigFloat80::fromDouble(float64_t value) {
	big_fi_t fi;
	fi.f64 = value;

	this->mantissa = (fi.i64 << 11) & 0x7FFFFFFFFFFFFFFF; //	left most bit is forced to 0

	if ((fi.i16 & 0x7FF0) == 0) {
		this->sign_exponent = 0xFFF0;
		return;
	}

	this->mantissa |= 0x8000000000000000; //	left most bit is now 1

	if ((fi.i16 & 0x7FF0) == 0x7FF0) {
		this->sign_exponent = fi.i16 | 0x000F;
		return;
	}

	this->sign_exponent = ((fi.i16 & 0x7FF0) >> 4) - 1023 + 16383 | (fi.i16 & 0x8000);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
BigFloat80& BigFloat80::operator=(const float64_t rhs) {
	this->fromDouble(rhs);
	return *this;
}

BigFloat80& BigFloat80::operator=(const uint32_t rhs) {
	this->fromDouble(rhs);
	return *this;
}

BigFloat80::operator float64_t() const { return this->toDouble(); }

const float64_t BigFloat80::operator()() const { return this->toDouble(); }


}
