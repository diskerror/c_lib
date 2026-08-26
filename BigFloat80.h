/*
 * Optimized BigFloat80 implementation.
 *
 * Design goals:
 * 1. Speed: Minimize byte-swapping by performing arithmetic on native registers.
 * 2. Speed: Remove 'union' type punning in favor of std::bit_cast (C++20/23).
 * 3. Safety: Ensure 10-byte alignment/packing.
 * 4. Readability: Cleaner native logic without implicit endian wrapper overhead during math.
 */

#ifndef DISKERROR_BIGFLOAT80G_H
#define DISKERROR_BIGFLOAT80G_H

#include <cmath>
#include <cstring> // std::memcpy
#include <bit>     // std::bit_cast
#include <boost/endian/arithmetic.hpp>

namespace Diskerror {

using namespace boost::endian;

// Ensure this structure is exactly 10 bytes with no padding.
#pragma pack(push, 1)

class BigFloat80 {
    // Storage is strictly Big Endian, matching sample rate-chunk in AIFF file format.
    big_uint16_t sign_exponent;
    big_uint64_t mantissa;

public:
    // =========================================================================
    // Constructors
    // =========================================================================
    
    BigFloat80() = default;
    BigFloat80(const BigFloat80&) = default;
    BigFloat80(BigFloat80&&) = default;
    
    BigFloat80& operator=(const BigFloat80&) = default;
    BigFloat80& operator=(BigFloat80&&) = default;

    inline BigFloat80(double v) { fromDouble(v); }
    inline BigFloat80(int64_t v) { fromDouble(static_cast<double>(v)); }
    inline BigFloat80(int v) { fromDouble(static_cast<double>(v)); }

    // Fast, dangerous copy from raw bytes (e.g. from file buffer).
    // Uses memcpy for potential compiler intrinsic optimization.
    explicit inline BigFloat80(const char* ptr) {
        std::memcpy(this, ptr, 10);
    }

    // =========================================================================
    // Conversions
    // =========================================================================

    /**
     * Converts the 80-bit big-endian float to a native double (64-bit).
     * Precision is lost (mantissa 63 -> 52 bits).
     */
    inline double toDouble() const {
        // 1. Load Big Endian storage into Native registers.
        //    This performs the byte swap exactly ONCE per variable.
        uint64_t m = mantissa;
        uint16_t se = sign_exponent;

        // 2. Handle specific "Un-normalized 1.0" case found in AIFFs.
        //    (Integer bit set, Exponent 0)
        if (m == 0x8000000000000000ULL && se == 0) {
            return 1.0;
        }

        // 3. Construct the double's bits using native arithmetic.
        
        // Extract Sign (Bit 15 of se -> Bit 63 of double)
        uint64_t sign_bit = (static_cast<uint64_t>(se) & 0x8000) << 48;

        // Extract Mantissa (Drop the explicit integer bit 'j-bit' at bit 63)
        // 80-bit: [I.FFFF...] (64 bits) -> 64-bit: [1.FFFF...] (Implied 1, 52 stored bits)
        // We take bits 62-11 from 80-bit to fill bits 51-0 of 64-bit.
        uint64_t mantissa64 = (m & 0x7FFFFFFFFFFFFFFFULL) >> 11;

        // Extract and Rebias Exponent
        // 80-bit Bias: 16383. 64-bit Bias: 1023.
        // Target = Source - 16383 + 1023
        int32_t exp80 = (se & 0x7FFF);
        uint64_t exp64 = 0;

        // Simple check to prevent underflow/overflow wrapping in simple cases
        // (A more robust impl would handle Inf/NaN/Denormal conversion explicitly here,
        //  but we match the logic of the original code for speed/consistency).
        if (exp80 != 0) { 
             exp64 = (static_cast<uint64_t>(exp80 - 16383 + 1023) & 0x7FF) << 52;
        }

        // 4. Combine and Bit Cast
        uint64_t result_bits = sign_bit | exp64 | mantissa64;
        return std::bit_cast<double>(result_bits);
    }

    /**
     * Converts a native double to 80-bit big-endian storage.
     */
    inline void fromDouble(double v) {
        // 1. Bit cast to integer to access raw IEEE-754 bits
        uint64_t bits = std::bit_cast<uint64_t>(v);

        // 2. Extract components
        uint16_t top_words = static_cast<uint16_t>(bits >> 48);
        uint16_t exp_bits = top_words & 0x7FF0;

        // 3. Prepare Mantissa (shift left to make room for precision expansion, though filled with 0)
        //    IEEE-754 double has implied 1. 80-bit has explicit 1.
        uint64_t m = (bits << 11) & 0x7FFFFFFFFFFFFFFFULL;

        // 4. Handle Special Cases based on Exponent
        if (exp_bits == 0) {
            // Zero or Denormal. Preserve the sign bit (for -0.0) and use a
            // zero exponent/mantissa so toDouble() reconstructs 0.0, not a
            // denormal-scaled garbage value.
            uint16_t sign = top_words & 0x8000;
            sign_exponent = sign;
            mantissa = 0;
            return;
        }
        
        // Add the explicit integer bit (J-bit) for Normalized numbers
        m |= 0x8000000000000000ULL;

        if (exp_bits == 0x7FF0) {
            // Infinity or NaN
            // Original logic: Keep sign, set exponent all 1s? 
            // Original: this->sign_exponent = fi.i16 | 0x000F; 
            // fi.i16 is top 16 bits. 0x7FF0 | 0x000F = 0x7FFF. 
            // So it preserves sign bit (0x8000) if present.
            sign_exponent = top_words | 0x000F;
            mantissa = m;
            return;
        }

        // 5. Normal Case: Rebias Exponent
        // Target = Source - 1023 + 16383
        uint16_t sign = top_words & 0x8000;
        uint16_t source_exp = (top_words & 0x7FF0) >> 4;
        uint16_t target_exp = source_exp - 1023 + 16383;
        
        sign_exponent = sign | target_exp;
        mantissa = m;
    }

    // =========================================================================
    // Operators
    // =========================================================================

    inline BigFloat80& operator=(double v) { 
        fromDouble(v); 
        return *this; 
    }
    
    inline BigFloat80& operator=(uint32_t v) { 
        fromDouble(static_cast<double>(v)); 
        return *this; 
    }

    inline explicit operator double() const { return toDouble(); }
    inline double operator()() const { return toDouble(); }
};

#pragma pack(pop)

typedef BigFloat80 big_float80_t;

} // namespace Diskerror

#endif // DISKERROR_BIGFLOAT80G_H
