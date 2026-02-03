//
// Optimized WindowedSinc implementation.
//

#ifndef DISKERROR_WINDOWEDSINC_H
#define DISKERROR_WINDOWEDSINC_H

#include <cmath>
#include <numbers>
#include <concepts>
#include <cstdint>
#include <numeric>      // std::transform_reduce
#include <functional>   // std::plus, std::multiplies
#include <iterator>     // std::random_access_iterator

#include "VectorMath.h"
#include "DiskerrorExceptions.h"

namespace Diskerror {

/**
 * Window function types for filter design.
 */
enum class WindowType {
    None,       // No window (rectangular)
    Blackman,
    Hamming
    // Hanning,   // TODO
    // Bartlett   // TODO
};

/**
 * Generates and manages a windowed sinc filter kernel.
 * Optimized to exploit symmetry (calculating only half the filter) and SIMD.
 *
 * Uses private inheritance from VectorMath to prevent accidental modification
 * of the filter kernel while still leveraging its arithmetic operations.
 */
template<typename T>
    requires std::floating_point<T>
class WindowedSinc : private VectorMath<T> {
    // Double precision for accurate coefficient computation in loops
    static constexpr double TWO_PI = 2.0 * std::numbers::pi;

    uint32_t M_ = 0;
    uint32_t Mo2_ = 0; // M / 2

    // Helper to calculate M based on transition width
    static uint32_t calculateM(T transition) {
        if (transition <= T(0) || transition >= T(0.5)) {
            throw UsageError("Transition band width out of range (0.0 < t < 0.5).");
        }

        // Smith's Guide Formula: M approx 4 / BW
        long m_long = std::lround(T(4) / transition);

        // Ensure M is even so the filter is symmetric around a center integer index.
        if (m_long % 2 != 0) m_long++;

        return static_cast<uint32_t>(m_long);
    }

    // Apply window function (internal helper)
    void applyWindow(WindowType window) {
        switch (window) {
            case WindowType::Blackman:
                applyBlackmanWindow();
                break;
            case WindowType::Hamming:
                applyHammingWindow();
                break;
            case WindowType::None:
            default:
                break;
        }
    }

    void applyBlackmanWindow() {
        T* d = this->data();
        // Use M_+2 so the zero-valued endpoints of the standard Blackman
        // window fall outside the kernel, avoiding wasted taps.
        const double div = static_cast<double>(M_ + 2);

        for (uint32_t i = 0; i <= Mo2_; ++i) {
            double ang = TWO_PI * static_cast<double>(i + 1) / div;
            T window_val = static_cast<T>(0.42 - 0.5 * std::cos(ang) + 0.08 * std::cos(2.0 * ang));

            d[i] *= window_val;

            if (i != Mo2_) {
                d[M_ - i] *= window_val;
            }
        }
        normalize();
    }

    void applyHammingWindow() {
        T* d = this->data();
        const double div = static_cast<double>(M_);

        for (uint32_t i = 0; i <= Mo2_; ++i) {
            T window_val = static_cast<T>(0.54 - 0.46 * std::cos(TWO_PI * static_cast<double>(i) / div));

            d[i] *= window_val;
            if (i != Mo2_) {
                d[M_ - i] *= window_val;
            }
        }
        normalize();
    }

public:
    // Expose read-only access from base class
    using VectorMath<T>::operator[];
    using VectorMath<T>::at;
    using VectorMath<T>::data;
    using VectorMath<T>::size;
    using VectorMath<T>::empty;
    using VectorMath<T>::begin;
    using VectorMath<T>::end;
    using VectorMath<T>::cbegin;
    using VectorMath<T>::cend;
    using VectorMath<T>::front;
    using VectorMath<T>::back;
    using VectorMath<T>::sum;
    using VectorMath<T>::max_mag;

    // Type aliases
    using typename VectorMath<T>::value_type;
    using typename VectorMath<T>::size_type;
    using typename VectorMath<T>::const_iterator;

    /**
     * Constructor
     *
     * @param cutoff_freq Cutoff frequency as fraction of sample rate (0.0 to 0.5)
     * @param transition  Transition band width as fraction of sample rate
     * @param window      Window function to apply (default: Blackman for good stopband)
     */
    WindowedSinc(T cutoff_freq, T transition, WindowType window = WindowType::Blackman)
        : M_(calculateM(transition)), Mo2_(M_ / 2)
    {
        if (cutoff_freq <= T(0) || cutoff_freq >= T(0.5)) {
            throw UsageError("Cut-off frequency out of range (0.0 < fc < 0.5).");
        }

        // 1. Resize VectorMath (Size is M + 1 to include endpoints [0, M])
        VectorMath<T>::resize(M_ + 1);

        // 2. Generate Sinc Function (Optimized for Symmetry)
        // Use double precision for accurate coefficient computation
        const double natFc = TWO_PI * static_cast<double>(cutoff_freq);
        T* d = this->data();

        // Calculate left half [0 ... Mo2 - 1] and mirror to right
        for (uint32_t i = 0; i < Mo2_; ++i) {
            double dist = static_cast<double>(Mo2_ - i);
            T val = static_cast<T>(std::sin(natFc * dist) / dist);

            d[i] = val;
            d[M_ - i] = val;
        }

        // Center value: limit of sin(x)/x as x->0 is 1, scaled by natFc
        d[Mo2_] = static_cast<T>(natFc);

        // 3. Normalize to unity gain at DC
        normalize();

        // 4. Apply window function
        applyWindow(window);
    }

    ~WindowedSinc() = default;

    [[nodiscard]] uint32_t getM() const noexcept { return M_; }
    [[nodiscard]] uint32_t getMo2() const noexcept { return Mo2_; }

    void normalize() {
        VectorMath<T>::normalize_sum(T(1));
    }

    /**
     * Converts Low Pass to High Pass (spectral inversion).
     * New(f) = 1 - H(f), implemented as Delta(n) - h(n) in time domain.
     */
    void makeLowCut() {
        VectorMath<T>::operator*=(T(-1));
        (*this)[Mo2_] += T(1);
    }

    /**
     * Fused Multiply-Sum (Convolution Step).
     * Calculates dot product of the kernel with a signal segment.
     *
     * @tparam InputIt  Iterator type (must be random access or forward iterator)
     * @param signal_begin Iterator to the start of the signal segment.
     *                     Must have at least (M + 1 - |offset|) valid elements.
     * @param offset Partial overlap adjustment for edge handling:
     *               0: Full kernel overlap (normal convolution)
     *               Negative: Use last |offset| kernel elements (signal at left edge)
     *               Positive: Use first |offset| kernel elements (signal at right edge)
     */
    template<std::random_access_iterator InputIt>
    [[nodiscard]] T fms(InputIt signal_begin, int32_t offset = 0) const {
        const auto kernel_size = static_cast<int32_t>(M_ + 1);

        if (std::abs(offset) >= kernel_size) {
            throw UsageError("Offset magnitude exceeds kernel size; no overlap remains.");
        }

        auto k_begin = this->cbegin();
        auto k_end = this->cend();

        if (offset < 0) {
            // Use tail of kernel: last |offset| elements
            k_begin = this->cend() + offset;
        } else if (offset > 0) {
            // Use head of kernel: first |offset| elements
            k_end = this->cbegin() + offset;
        }

        return std::transform_reduce(
            k_begin, k_end,
            signal_begin,
            T(0),
            std::plus<>(),
            std::multiplies<>()
        );
    }
};

} // namespace Diskerror

#endif // DISKERROR_WINDOWEDSINC_H
