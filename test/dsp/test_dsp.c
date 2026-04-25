#include "unity.h"

#include "dsp/filters.h"
#include "dsp/window.h"

#include <math.h>

void setUp(void) {}
void tearDown(void) {}

void test_hann_endpoints_are_zero(void)
{
    enum { N = 2048 };
    float window[N];
    window_make_hann(window, N);

    TEST_ASSERT_EQUAL_FLOAT(0.0f, window[0]);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, window[N - 1]);
}

void test_hann_is_symmetric(void)
{
    enum { N = 2048 };
    float window[N];
    window_make_hann(window, N);

    for (SizeType i = 0; i < N / 2; ++i) {
        TEST_ASSERT_EQUAL_FLOAT(window[i], window[N - 1 - i]);
    }
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_hann_endpoints_are_zero);
    RUN_TEST(test_hann_is_symmetric);

    return UNITY_END();
}
