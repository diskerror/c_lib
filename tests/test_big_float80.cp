// Unit tests for Diskerror::BigFloat80 — 80-bit big-endian float <-> double.

#include "../BigFloat80.h"

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

static bool approx(double a, double b, double eps = 1e-6) {
    return std::abs(a - b) <= eps;
}

int main() {
    // Common AIFF sample rates round-trip through double conversion
    double rates[] = {44100.0, 48000.0, 96000.0, 22050.0, 8000.0, 192000.0};
    for (double r : rates) {
        BigFloat80 bf(r);
        check(approx(bf.toDouble(), r), "round-trip sample rate");
    }

    // Zero
    BigFloat80 zero(0.0);
    check(approx(zero.toDouble(), 0.0), "zero round-trips to 0.0");

    // Negative value
    BigFloat80 neg(-123.5);
    check(approx(neg.toDouble(), -123.5), "negative value round-trips");

    // Integer constructors
    BigFloat80 fromInt(static_cast<int64_t>(44100));
    check(approx(fromInt.toDouble(), 44100.0), "int64_t constructor");

    BigFloat80 fromIntSmall(48000);
    check(approx(fromIntSmall.toDouble(), 48000.0), "int constructor");

    // Assignment operators
    BigFloat80 assigned;
    assigned = 96000.0;
    check(approx(assigned.toDouble(), 96000.0), "operator= double");

    assigned = static_cast<uint32_t>(44100);
    check(approx(assigned.toDouble(), 44100.0), "operator= uint32_t");

    // operator double() and operator()()
    BigFloat80 conv(44100.0);
    check(approx(static_cast<double>(conv), 44100.0), "explicit operator double()");
    check(approx(conv(), 44100.0), "operator()()");

    // Struct is exactly 10 bytes (packed, matches AIFF COMM sampleRate field)
    check(sizeof(BigFloat80) == 10, "sizeof(BigFloat80) == 10 bytes");

    if (failures == 0) {
        std::printf("All BigFloat80 tests passed.\n");
        return 0;
    }
    std::printf("%d BigFloat80 test(s) failed.\n", failures);
    return 1;
}
