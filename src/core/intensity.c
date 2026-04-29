#include "intensity.h"

#include <math.h>

static float clamp_unit(float x)
{
    return fminf(fmaxf(x, 0.0f), 1.0f);
}

float intensity_from_bin(Complex bin, float power_reference, float min_db)
{
    const float re = crealf(bin);
    const float im = cimagf(bin);
    const float power = re * re + im * im;

    // +ε keeps log10 well-defined for silent bins (power → 0 ⇒ db → min_db).
    const float db = 10.0f * log10f((power / power_reference) + 1e-9f);

    return clamp_unit((db - min_db) / -min_db);
}
