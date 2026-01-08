//
// Optimized WindowedSinc implementation.
//

#ifndef DISKERROR_WINDOWEDSINC_H
#define DISKERROR_WINDOWEDSINC_H

#include <cmath>
#include <numbers>
#include <stdexcept>
#include <concepts>
#include <algorithm> // for std::copy

#include "VectorMath.h"

namespace Diskerror {

using namespace std;

/**
 * Generates and manages a list of numbers in the form of a windowed sinc function.
 * Optimized to exploit symmetry (calculating only half the filter) and SIMD.
 */
template<typename T>
    requires std::floating_point<T> // C++20 Concept
class WindowedSinc : public VectorMath<T> {
    static constexpr T PI = numbers::pi_v<T>;
    static constexpr T TWO_PI = 2.0 * PI;

    uint32_t M = 0;
    uint32_t Mo2 = 0; // M / 2

    // =========================================================================
    // API Restrictions (Hiding Vector Mutators)
    // =========================================================================
    // We privately inherit or explicitly delete/hide these to prevent 
    // accidental resizing of the filter kernel after construction.
    
    using VectorMath<T>::operator=;
    using VectorMath<T>::assign;
    using VectorMath<T>::assign_range;
    using VectorMath<T>::push_back;
    using VectorMath<T>::pop_back;
    using VectorMath<T>::resize;
    using VectorMath<T>::clear;
    // ... (Other mutators can be hidden as needed)

    // Helper to calculate M based on transition width
    static uint32_t calculateM(const T transition) {
        if (transition <= T(0) || transition >= T(0.5)) {
            throw runtime_error("Transition band width out of range (0.0 < t < 0.5).");
        }
        
        // Smith's Guide Formula: M approx 4 / BW
        long m_long = lround(4.0 / transition);
        
        // Ensure M is even so the filter is symmetric around a center integer index.
        if (m_long % 2 != 0) m_long++;
        
        return static_cast<uint32_t>(m_long);
    }

public:
    /**
     * Constructor
     * 
     * @param cutoff_freq Cutoff frequency as fraction of sample rate (0.0 to 0.5)
     * @param transition  Transition band width as fraction of sample rate
     */
    WindowedSinc(const T cutoff_freq, const T transition) 
        : M(calculateM(transition)), Mo2(calculateM(transition) / 2) 
    {
        if (cutoff_freq <= T(0) || cutoff_freq >= T(0.5)) {
            throw runtime_error("Cut-off frequency out of range (0.0 < fc < 0.5).");
        }

        // 1. Resize VectorMath
        // Size is M + 1 to include the endpoints [0, M]
        VectorMath<T>::resize(this->M + 1);

        // 2. Generate Sinc Function (Optimized for Symmetry)
        // Sinc is even symmetric: f(x) = f(-x)
        // We calculate from 0 to Mo2 (center), then mirror to the right side.
        
        const T natFc = TWO_PI * cutoff_freq; // Normalized angular cutoff
        T* data = this->data(); // Direct pointer access for speed

        // Calculate left half [0 ... Mo2 - 1]
        for (uint32_t i = 0; i < this->Mo2; ++i) {
            // i_m_mo2 is negative here: i - Mo2
            // sin(a * -x) / -x  == -sin(ax) / -x == sin(ax) / x
            // We can treat distance as positive for calculation
            T dist = static_cast<T>(this->Mo2 - i);
            T val = std::sin(natFc * dist) / dist;
            
            data[i] = val;
            data[this->M - i] = val; // Mirror to right side
        }

        // Calculate Center [Mo2]
        // Limit of sin(x)/x as x->0 is 1. Here we multiply by 2*pi*fc.
        // So center value is 2*pi*fc.
        data[this->Mo2] = natFc;

        // 3. Normalize Sum to 1.0 (Unity Gain at DC)
        this->normalize();
    }

    ~WindowedSinc() = default;

    [[nodiscard]] uint32_t getM() const noexcept { return this->M; }
    [[nodiscard]] uint32_t getMo2() const noexcept { return this->Mo2; }

    void normalize() { 
        this->normalize_sum(T(1)); 
    }

    // =========================================================================
    // Window Functions
    // =========================================================================

    /**
     * Applies Blackman Window.
     * Logic optimized for symmetry: w[i] == w[M-i]
     */
    void ApplyBlackman() {
        T* data = this->data();
        const double div = static_cast<double>(this->M + 2); // Matches original math

        // Loop up to center inclusive
        for (uint32_t i = 0; i <= this->Mo2; ++i) {
            // Original Formula: 2*pi * (i+1) / (M+2)
            double ang = TWO_PI * (i + 1) / div;
            
            T window_val = static_cast<T>(0.42 - (0.5 * std::cos(ang)) + (0.08 * std::cos(2.0 * ang)));
            
            data[i] *= window_val;
            
            // Mirror to right side (avoid double writing center)
            if (i != this->Mo2) {
                data[this->M - i] *= window_val;
            }
        }
        this->normalize(); // Re-normalize after windowing
    }

    /**
     * Applies Hamming Window.
     * Logic optimized for symmetry.
     */
    void ApplyHamming() {
        T* data = this->data();
        const double div = static_cast<double>(this->M);

        for (uint32_t i = 0; i <= this->Mo2; ++i) {
             // Standard Hamming: 0.54 - 0.46 * cos(2*pi*i / M)
            T window_val = static_cast<T>(0.54 - (0.46 * std::cos(TWO_PI * i / div)));
            
            data[i] *= window_val;
            if (i != this->Mo2) {
                data[this->M - i] *= window_val;
            }
        }
        this->normalize();
    }

	//  TODO:
	// void ApplyHanning() {}

	//  TODO:
	// void ApplyBartlett() {}


    /**
     * Converts Low Pass to Low Cut (High Pass).
     * Inverts spectral shape: New(f) = 1 - H(f)
     * In time domain: Delta(n) - h(n)
     */
    void MakeLowCut() {
        // Invert all taps
        this->operator*=(T(-1));
        // Add 1.0 to the center sample (Dirac delta at t=0)
        (*this)[this->Mo2] += T(1);
    }

    /**
     * Fused Multiply Sum (Convolution Step).
     * Calculates dot product of the kernel with a signal segment.
     * 
     * @param begin2 Iterator to the signal input.
     * @param offset Shift of the kernel relative to signal.
     *               0: Full overlap.
     *               Positive: Kernel shifted right (uses end of kernel).
     *               Negative: Kernel shifted left (uses start of kernel).
     */
    T fms(auto begin2, int32_t offset = 0) const {
        if (std::abs(offset) >= static_cast<int32_t>(this->M)) {
             throw runtime_error("'offset' is too big, kernel no longer overlaps.");
        }

        auto k_begin = this->begin();
        auto k_end   = this->end();

        // Adjust kernel range based on offset
        if (offset < 0) {
            // Offset negative: Kernel is shifted "left" against the signal?
            // Original code: begin1 = end() + n. 
            // e.g., n=-1 => uses last 1 element.
            k_begin = this->end() + offset;
        } else if (offset > 0) {
            // Offset positive: 
            // Original code: end1 = begin() + n.
            // e.g., n=1 => uses first 1 element.
            k_end = this->begin() + offset;
        }

        // Use standard inner_product or transform_reduce
        // begin2 is assumed to point to valid memory of sufficient length.
        return std::transform_reduce(
            k_begin, k_end, 
            begin2, 
            T(0), 
            std::plus<>(), 
            std::multiplies<>()
        );
    }
};

} // namespace Diskerror

#endif // DISKERROR_WINDOWEDSINC_H
