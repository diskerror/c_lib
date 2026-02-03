//
// Created by Reid Woodbury on 11/15/24.
//

#ifndef DISKERROR_VECTORMATH_H
#define DISKERROR_VECTORMATH_H

#include <vector>
#include <numeric>          // std::reduce, std::transform_reduce
#include <algorithm>        // std::max_element, std::min_element, std::max
#include <cmath>            // std::abs
#include <limits>           // std::numeric_limits
#include <stdexcept>        // std::invalid_argument
#include <initializer_list>

namespace Diskerror {

/**
 * Optimized VectorMath for off-line audio processing.
 * Behaves like std::valarray but uses std::vector internally via composition.
 *
 * DESIGN GOALS:
 * 1. SIMD Friendly: Uses std::reduce/transform_reduce to allow out-of-order execution.
 * 2. Efficiency: Replaces division with inverse-multiplication in loops.
 * 3. Correctness: Full const-correctness and noexcept specifications.
 *
 * Note: Arithmetic operations assume T is a floating-point or numeric type
 * whose basic operations do not throw.
 */
template <typename T>
class VectorMath {
    std::vector<T> data_;

public:
    // Type aliases (matching std::vector conventions)
    using value_type      = T;
    using size_type       = typename std::vector<T>::size_type;
    using difference_type = typename std::vector<T>::difference_type;
    using reference       = typename std::vector<T>::reference;
    using const_reference = typename std::vector<T>::const_reference;
    using iterator        = typename std::vector<T>::iterator;
    using const_iterator  = typename std::vector<T>::const_iterator;

    // =========================================================================
    // Constructors
    // =========================================================================

    VectorMath() = default;

    explicit VectorMath(size_type count) : data_(count) {}

    VectorMath(size_type count, const T& value) : data_(count, value) {}

    VectorMath(std::initializer_list<T> init) : data_(init) {}

    template <typename InputIt>
    VectorMath(InputIt first, InputIt last) : data_(first, last) {}

    explicit VectorMath(const std::vector<T>& vec) : data_(vec) {}

    explicit VectorMath(std::vector<T>&& vec) noexcept : data_(std::move(vec)) {}

    // =========================================================================
    // Element Access
    // =========================================================================

    reference       operator[](size_type i) noexcept { return data_[i]; }
    const_reference operator[](size_type i) const noexcept { return data_[i]; }

    reference       at(size_type i) { return data_.at(i); }
    const_reference at(size_type i) const { return data_.at(i); }

    T*       data() noexcept { return data_.data(); }
    const T* data() const noexcept { return data_.data(); }

    reference       front() noexcept { return data_.front(); }
    const_reference front() const noexcept { return data_.front(); }

    reference       back() noexcept { return data_.back(); }
    const_reference back() const noexcept { return data_.back(); }

    // =========================================================================
    // Iterators
    // =========================================================================

    iterator       begin() noexcept { return data_.begin(); }
    const_iterator begin() const noexcept { return data_.begin(); }
    const_iterator cbegin() const noexcept { return data_.cbegin(); }

    iterator       end() noexcept { return data_.end(); }
    const_iterator end() const noexcept { return data_.end(); }
    const_iterator cend() const noexcept { return data_.cend(); }

    // =========================================================================
    // Capacity
    // =========================================================================

    [[nodiscard]] bool      empty() const noexcept { return data_.empty(); }
    [[nodiscard]] size_type size() const noexcept { return data_.size(); }
    [[nodiscard]] size_type capacity() const noexcept { return data_.capacity(); }

    void reserve(size_type new_cap) { data_.reserve(new_cap); }
    void resize(size_type count) { data_.resize(count); }
    void resize(size_type count, const T& value) { data_.resize(count, value); }
    void shrink_to_fit() { data_.shrink_to_fit(); }
    void clear() noexcept { data_.clear(); }

    // =========================================================================
    // Modifiers
    // =========================================================================

    void push_back(const T& value) { data_.push_back(value); }
    void push_back(T&& value) { data_.push_back(std::move(value)); }
    void pop_back() noexcept { data_.pop_back(); }

    template <typename... Args>
    reference emplace_back(Args&&... args) {
        return data_.emplace_back(std::forward<Args>(args)...);
    }

    void swap(VectorMath& other) noexcept { data_.swap(other.data_); }

    // =========================================================================
    // Arithmetic Modifiers (return reference for chaining)
    // =========================================================================

    VectorMath& operator*=(const T& a) noexcept {
        for (auto& elem : data_) {
            elem *= a;
        }
        return *this;
    }

    VectorMath& operator/=(const T& a) {
        if (a == T(0)) {
            throw std::invalid_argument("VectorMath: division by zero");
        }
        const T inv = T(1) / a;
        for (auto& elem : data_) {
            elem *= inv;
        }
        return *this;
    }

    void normalize_sum(const T max_possible) noexcept {
        const T s = sum();
        if (s != T(0)) {
            *this *= (max_possible / s);
        }
    }

    void normalize_mag(const T max_possible) noexcept {
        const T m = max_mag();
        if (m != T(0)) {
            *this *= (max_possible / m);
        }
    }

    // =========================================================================
    // Accessors (Const Correct)
    // =========================================================================

    [[nodiscard]] T sum() const noexcept {
        if (data_.empty()) return T(0);
        // std::reduce (C++17) allows SIMD optimizations.
        return std::reduce(data_.begin(), data_.end(), T(0));
    }

    [[nodiscard]] T average() const noexcept {
        if (data_.empty()) return T(0);
        // Use inverse-multiplication for efficiency.
        const T inv_size = T(1) / static_cast<T>(data_.size());
        return sum() * inv_size;
    }

    [[nodiscard]] T max() const noexcept {
        if (data_.empty()) return std::numeric_limits<T>::lowest();
        return *std::max_element(data_.begin(), data_.end());
    }

    [[nodiscard]] T min() const noexcept {
        if (data_.empty()) return std::numeric_limits<T>::max();
        return *std::min_element(data_.begin(), data_.end());
    }

    [[nodiscard]] T max_mag() const noexcept {
        if (data_.empty()) return T(0);

        // std::transform_reduce (C++17) fused operation.
        return std::transform_reduce(
            data_.begin(), data_.end(),
            T(0),
            [](const T& a, const T& b) { return std::max(a, b); },
            [](const T& a) { return std::abs(a); }
        );
    }
};

} // namespace Diskerror

#endif // DISKERROR_VECTORMATH_H
