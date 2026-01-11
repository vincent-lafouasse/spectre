#pragma once

#include <raylib.h>

#include "core/History.h"
#include "definitions.h"

typedef struct {
    // the texture is contains the same info as the history
    // except it's on the GPU
    Texture2D texture;
    float height, width;
    Vector2 origin;
    SizeType size;
} RMSVisualizer;

RMSVisualizer rms_vis_new(SizeType size, float w, float h, Vector2 origin);
void rms_vis_destroy(RMSVisualizer* rv);
void rms_vis_update(RMSVisualizer* rv,
                    const FloatHistory* rms_history,
                    SizeType n);
void rms_vis_render_static(const RMSVisualizer* rv,
                           const FloatHistory* rms_history);
