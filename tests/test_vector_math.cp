// Unit tests for Diskerror::VectorMath — arithmetic modifiers and reductions.

#include "../VectorMath.h"

#include <cmath>
#include <cstdio>

using namespace Diskerror;

static int failures = 0;

static void check(bool cond, const char* what) {
    if (!cond) {
        std::printf("FAIL %s\n", what);
        ++failures;
    }
}

static bool approx(double a, double b, double eps = 1e-9) {
    return std::abs(a - b) <= eps;
}

int main() {
    // Construction & basic accessors
    VectorMath<double> v{1.0, 2.0, 3.0, 4.0};
    check(v.size() == 4, "size after init-list ctor");
    check(approx(v[0], 1.0), "operator[] read");
    check(approx(v.front(), 1.0), "front()");
    check(approx(v.back(), 4.0), "back()");

    // sum / average / max / min
    check(approx(v.sum(), 10.0), "sum()");
    check(approx(v.average(), 2.5), "average()");
    check(approx(v.max(), 4.0), "max()");
    check(approx(v.min(), 1.0), "min()");

    // max_mag with negative values
    VectorMath<double> neg{-5.0, 1.0, 3.0};
    check(approx(neg.max_mag(), 5.0), "max_mag() with negative");

    // operator*=
    VectorMath<double> m{1.0, 2.0, 3.0};
    m *= 2.0;
    check(approx(m[0], 2.0) && approx(m[1], 4.0) && approx(m[2], 6.0), "operator*=");

    // operator/=
    m /= 2.0;
    check(approx(m[0], 1.0) && approx(m[1], 2.0) && approx(m[2], 3.0), "operator/=");

    // division by zero throws
    bool threw = false;
    try {
        m /= 0.0;
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    check(threw, "operator/= throws on zero");

    // normalize_mag
    VectorMath<double> nm{-2.0, 1.0, 4.0};
    nm.normalize_mag(1.0);
    check(approx(nm.max_mag(), 1.0), "normalize_mag() scales to target max");

    // normalize_sum
    VectorMath<double> ns{1.0, 1.0, 2.0};
    ns.normalize_sum(8.0);
    check(approx(ns.sum(), 8.0), "normalize_sum() scales to target sum");

    // empty vector edge cases
    VectorMath<double> e;
    check(e.empty(), "empty() true for default ctor");
    check(approx(e.sum(), 0.0), "sum() of empty is 0");
    check(approx(e.average(), 0.0), "average() of empty is 0");
    check(approx(e.max_mag(), 0.0), "max_mag() of empty is 0");

    // push_back / resize
    VectorMath<int> pb;
    pb.push_back(5);
    pb.push_back(6);
    check(pb.size() == 2 && pb[0] == 5 && pb[1] == 6, "push_back()");
    pb.resize(4, 9);
    check(pb.size() == 4 && pb[2] == 9 && pb[3] == 9, "resize() with fill value");

    if (failures == 0) {
        std::printf("All VectorMath tests passed.\n");
        return 0;
    }
    std::printf("%d VectorMath test(s) failed.\n", failures);
    return 1;
}
