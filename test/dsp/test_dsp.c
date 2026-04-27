#include "unity.h"

#include "dsp/filters.h"
#include "dsp/window.h"

#include <math.h>

void setUp(void) {}
void tearDown(void) {}

void test_hann_coherent_gain(void)
{
    enum { N = 2048 };
    float window[N];
    window_make_hann(window, N);

    float sum = 0.0f;
    for (SizeType i = 0; i < N; ++i) {
        sum += window[i];
    }
    const float actual_coherent_gain = sum / (float)N;

    // Closed form: 0.5·(N−1)/N — *not* 0.5 exactly. Difference ≈ 1/(2N).
    const float expected_coherent_gain = 0.5f * (float)(N - 1) / (float)N;

    // Naive-sum forward error bound (Higham): N·ε·Σ|x_i| ≈ 2.5e-4 absolute on
    // the sum, ≈1.2e-4 absolute on CG. Empirical is closer to 1e-5; we leave
    // headroom so this never flakes on float noise.
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, actual_coherent_gain,
                             expected_coherent_gain);
}

void test_window_apply_unit_signal_yields_window(void)
{
    // Multiplying by 1.0f is bit-exact, so applying the window to a DC signal
    // of value 1 reproduces the window verbatim. Strict equality is fair.
    enum { N = 64 };
    float data[N], window[N];
    for (SizeType i = 0; i < N; ++i) {
        data[i] = 1.0f;
    }
    window_make_hann(window, N);
    window_apply(data, window, N);

    for (SizeType i = 0; i < N; ++i) {
        TEST_ASSERT_EQUAL_FLOAT(window[i], data[i]);
    }
}

void test_window_apply_zero_signal_yields_zero(void)
{
    // 0 · anything must be 0. Catches anything weird in window_apply doing
    // more than a multiplication.
    enum { N = 64 };
    float data[N], window[N];
    for (SizeType i = 0; i < N; ++i) {
        data[i] = 0.0f;
    }
    window_make_hann(window, N);
    window_apply(data, window, N);

    for (SizeType i = 0; i < N; ++i) {
        TEST_ASSERT_EQUAL_FLOAT(0.0f, data[i]);
    }
}

void test_window_power_reference_hann(void)
{
    enum { N = 2048 };
    float window[N];
    window_make_hann(window, N);

    const float pref = window_power_reference(window, N);
    const float exact = 4.0f / ((float)(N - 1) * (float)(N - 1));

    // Expected value is ~9.5e-7. Absolute tolerance is meaningless at that
    // magnitude — use a relative one (1e-3 of expected).
    TEST_ASSERT_FLOAT_WITHIN(exact * 1e-3f, exact, pref);
}

void test_filter_hpf_removes_dc_from_mixed_signal(void)
{
    // An LTI filter with H(0)=0 outputs zero mean in steady state for *any*
    // input. Build a sine-plus-DC signal, run it through the HPF, let the
    // transient die, and check the windowed mean is ~0. f_signal is chosen so
    // an integer number of cycles fits in the analysis window — that kills
    // truncation error from a partial period; anything left is float noise.
    enum { N = 4096 };

    const float fs = 48000.0f;
    const float fc = 50.0f;
    const float f_signal =
        750.0f;  // 64 spp → 32 full cycles over [TRANSIENT, N)
    const float dc_offset = 2.0f;

    float data[N];
    for (SizeType i = 0; i < N; ++i) {
        data[i] = sinf(2.0f * PI * f_signal * (float)i / fs) + dc_offset;
    }

    OnePoleFilter f = filter_init(fc, fs);
    filter_hpf_process(&f, data, N);

    // Verify our skip window is at least 7τ — guarantees the un-blocked DC
    // residue is ≤ e^-7 ≈ 0.09 % (-60 dB) before we start measuring. For the
    // impulse-invariant α used in filter_init, τ in samples is just the
    // angular period of the cutoff: τ = fs / (2π·fc).
    const float tau = fs / (2.0f * PI * fc);
    const SizeType transient = N / 2;
    TEST_ASSERT_GREATER_THAN_FLOAT(7.0f * tau, (float)transient);

    float sum = 0.0f;
    for (SizeType i = transient; i < N; ++i) {
        sum += data[i];
    }
    const float mean = sum / (float)(N - transient);

    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.0f, mean);
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_hann_coherent_gain);

    RUN_TEST(test_window_apply_unit_signal_yields_window);
    RUN_TEST(test_window_apply_zero_signal_yields_zero);

    RUN_TEST(test_window_power_reference_hann);

    RUN_TEST(test_filter_hpf_removes_dc_from_mixed_signal);

    return UNITY_END();
}
