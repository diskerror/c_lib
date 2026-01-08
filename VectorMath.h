//
// Created by Reid Woodbury on 11/15/24.
//

#ifndef DISKERROR_VECTORMATH3_H
#define DISKERROR_VECTORMATH3_H

#include <vector>
#include <numeric>   // std::reduce, std::transform_reduce
#include <algorithm> // std::max_element, std::min_element, std::max
#include <cmath>     // std::abs
#include <limits>    // std::numeric_limits
#include <cstdint>   // uint64_t

namespace Diskerror {

/**
 * Optimized VectorMath for off-line audio processing.
 *
 * DESIGN GOALS:
 * 1. SIMD Friendly: Uses std::reduce/transform_reduce to allow out-of-order execution.
 * 2. Efficiency: Replaces division with inverse-multiplication in loops.
 * 3. Correctness: Full const-correctness and noexcept specifications.
 */
template <typename T>
class VectorMath : public std::vector<T> {
public:
    // Inherit all std::vector constructors
    using std::vector<T>::vector;

    // =========================================================================
    // Modifiers (return reference for chaining)
    // =========================================================================

    VectorMath& operator*=(const T& a) noexcept {
        for (auto& elem : *this) {
            elem *= a;
        }
        return *this;
    }

    VectorMath& operator/=(const T& a) {
        if (a != T(0)) {
            const T inv = T(1) / a;
            for (auto& elem : *this) {
                elem *= inv;
            }
        }
        return *this;
    }

    void normalize_sum(const T max_possible) {
        const T s = this->sum();
        if (s != T(0)) {
            *this *= (max_possible / s);
        }
    }

    void normalize_mag(const T max_possible) {
        const T m = this->max_mag();
        if (m != T(0)) {
            *this *= (max_possible / m);
        }
    }

    // =========================================================================
    // Accessors (Const Correct)
    // =========================================================================

    [[nodiscard]] T sum() const noexcept {
        if (this->empty()) return T(0);
        // std::reduce (C++17) allows SIMD optimizations.
        return std::reduce(this->begin(), this->end(), T(0));
    }

    [[nodiscard]] T average() const noexcept {
        if (this->empty()) return T(0);
        return this->sum() / static_cast<T>(this->size());
    }

    [[nodiscard]] T max() const noexcept {
        if (this->empty()) return std::numeric_limits<T>::lowest();
        return *std::max_element(this->begin(), this->end());
    }

    [[nodiscard]] T min() const noexcept {
        if (this->empty()) return std::numeric_limits<T>::max();
        return *std::min_element(this->begin(), this->end());
    }

    [[nodiscard]] T max_mag() const noexcept {
        if (this->empty()) return T(0);

        // std::transform_reduce (C++17) fused operation.
        return std::transform_reduce(
            this->begin(), this->end(),
            T(0),
            [](const T& a, const T& b) { return std::max(a, b); },
            [](const T& a) { return std::abs(a); }
        );
    }
};

} // namespace Diskerror

#endif // DISKERROR_VECTORMATH3_H
