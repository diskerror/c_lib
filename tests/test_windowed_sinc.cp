// Unit tests for Diskerror::WindowedSinc — kernel generation and convolution.

#include "../WindowedSinc.h"
#include "../DiskerrorExceptions.h"

#include <cmath>
#include <cstdio>
#include <vector>

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
    // Basic construction (Blackman default)
    WindowedSinc<double> lp(0.25, 0.1);
    check(lp.getM() % 2 == 0, "M is even (symmetric kernel)");
    check(lp.size() == lp.getM() + 1, "size() == M + 1");

    // Kernel should sum to ~1.0 (unity gain at DC) after normalize()
    check(approx(lp.sum(), 1.0, 1e-6), "kernel normalized to unity gain");

    // Symmetry: kernel[i] == kernel[M - i]
    bool symmetric = true;
    for (uint32_t i = 0; i <= lp.getMo2(); ++i) {
        if (!approx(lp[i], lp[lp.getM() - i], 1e-9)) {
            symmetric = false;
            break;
        }
    }
    check(symmetric, "kernel is symmetric");

    // Hamming window variant constructs without throwing
    bool hamming_ok = true;
    try {
        WindowedSinc<double> hw(0.2, 0.15, WindowType::Hamming);
        hamming_ok = approx(hw.sum(), 1.0, 1e-6);
    } catch (...) {
        hamming_ok = false;
    }
    check(hamming_ok, "Hamming window constructs and normalizes");

    // None window variant
    bool none_ok = true;
    try {
        WindowedSinc<double> nw(0.2, 0.15, WindowType::None);
        none_ok = approx(nw.sum(), 1.0, 1e-6);
    } catch (...) {
        none_ok = false;
    }
    check(none_ok, "None window constructs and normalizes");

    // Out-of-range cutoff throws
    bool threw_cutoff = false;
    try {
        WindowedSinc<double> bad(0.6, 0.1);
    } catch (const UsageError&) {
        threw_cutoff = true;
    }
    check(threw_cutoff, "out-of-range cutoff throws UsageError");

    // Out-of-range transition throws
    bool threw_transition = false;
    try {
        WindowedSinc<double> bad(0.25, 0.6);
    } catch (const UsageError&) {
        threw_transition = true;
    }
    check(threw_transition, "out-of-range transition throws UsageError");

    // fms(): convolving with an impulse-like signal returns a kernel tap
    std::vector<double> signal(lp.size(), 0.0);
    signal[lp.getMo2()] = 1.0; // impulse at center
    double result = lp.fms(signal.begin());
    check(approx(result, lp[lp.getMo2()], 1e-9), "fms() dot product on impulse matches center tap");

    // makeLowCut: DC response should now be ~0 instead of ~1
    WindowedSinc<double> hp(0.25, 0.1);
    hp.makeLowCut();
    check(approx(hp.sum(), 0.0, 1e-6), "makeLowCut() zeroes DC gain");

    if (failures == 0) {
        std::printf("All WindowedSinc tests passed.\n");
        return 0;
    }
    std::printf("%d WindowedSinc test(s) failed.\n", failures);
    return 1;
}
