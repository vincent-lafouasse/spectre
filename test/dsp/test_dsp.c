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
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, actual_coherent_gain, expected_coherent_gain);
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_hann_coherent_gain);

    return UNITY_END();
}
