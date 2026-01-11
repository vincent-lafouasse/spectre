#include "RMSVisualizer.h"

#include <math.h>

#include "colormap/colormap.h"

static float clamp_unit(float f)
{
    return fminf(fmaxf(f, 0.0f), 1.0f);
}

static Color float_to_color(float intensity,
                            const uint8_t (*const cmap)[4],
                            SizeType cmap_size)
{
    const float clamped = clamp_unit(intensity);
    const int index = (int)(clamped * (cmap_size - 0.0001f));
    return *(Color*)cmap[index];
}

RMSVisualizer rms_vis_new(SizeType size, float w, float h, Vector2 origin)
{
    Image img = GenImageColor(size, 1, BLACK);
    Texture2D texture = LoadTextureFromImage(img);
    UnloadImage(img);
    SetTextureFilter(texture, TEXTURE_FILTER_BILINEAR);

    return (RMSVisualizer){
        .texture = texture,
        .height = h,
        .width = w,
        .origin = origin,
        .size = size,
    };
}

void rms_vis_destroy(RMSVisualizer* rv)
{
    if (!rv) {
        return;
    }

    UnloadTexture(rv->texture);
}

static void rms_vis_update_value(RMSVisualizer* rv, float value, SizeType index)
{
    const uint8_t(*const cmap)[4] = plasma_rgba;
    value = clamp_unit(value);
    const Color color = float_to_color(value, cmap, COLORMAP_SIZE);
    const Color* pixels = &color;  // 1 pixel

    UpdateTextureRec(rv->texture, (Rectangle){(float)index, 0, 1, 1}, pixels);
}

// called after processing and before drawing block
void rms_vis_update(RMSVisualizer* rv,
                    const FloatHistory* rms_history,
                    SizeType n)
{
    n = n > rms_history->cap ? rms_history->cap : n;
    const SizeType start =
        (rms_history->cap + rms_history->tail - n) % rms_history->cap;

    for (SizeType i = 0; i < n; i++) {
        const SizeType index = (start + i) % rms_history->cap;
        const float value = rms_history->data[index];
        rms_vis_update_value(rv, value, index);
    }
}

void rms_vis_render_wrap(const RMSVisualizer* rv,
                         const FloatHistory* rms_history)
{
    const float band_width = rv->width / rms_history->cap;

    for (SizeType i = 0; i < rms_history->cap; i++) {
        if (rms_history->len < rms_history->cap && i >= rms_history->tail) {
            break;
        }

        // stretch this
        const Rectangle src = {(float)i, 0, 1, 1};

        // to this
        const float height = clamp_unit(rms_history->data[i]) * rv->height;
        const Vector2 corner = {
            i * band_width,
            0.5f * (rv->height - height),
        };
        const Rectangle dest = {
            corner.x,
            corner.y,
            band_width,
            height,
        };

        const float angle = 0.0f;
        const Color tint = WHITE;  // no tint
        DrawTexturePro(rv->texture, src, dest, rv->origin, angle, tint);
    }
}
