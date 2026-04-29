#pragma once

#include "definitions.h"

// Map a complex FFT bin to a dimensionless intensity in [0, 1].
//
//   - power_reference: |X|² that should map to 1.0 — defines the 0 dB anchor.
//   - min_db:          dB value that should map to 0.0 — defines the floor.
//
// The output is clamped to [0, 1]. Silent bins (power → 0) map to 0 via a
// tiny epsilon that keeps log10 well-defined.
float intensity_from_bin(Complex bin, float power_reference, float min_db);
