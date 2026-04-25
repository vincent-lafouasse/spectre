#include "unity.h"

#include "dsp/window.h"

void setUp(void) {}
void tearDown(void) {}

void test_dummy(void)
{
    float window[5];
    window_make_hann(window, 5);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_dummy);
    return UNITY_END();
}
