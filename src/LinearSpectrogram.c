#include "LinearSpectrogram.h"

#include <math.h>
#include <stdlib.h>

#include "core/History.h"

LinearSpectrogramConfig linear_spectrogram_config(Rectangle screen,
                                                  Colormap cmap,
                                                  const FFTConfig* analyzer_cfg)
{
    const SizeType fft_size = analyzer_cfg->size;
    const SizeType logical_height = fft_size / 2;
    const SizeType logical_width = analyzer_cfg->history_size;

    const float power_reference = 0.25f * (float)(fft_size * fft_size);
    const float min_dB = -60.0f;  // -60 dB should be quiet enough

    return (LinearSpectrogramConfig){
        .screen = screen,
        .logical_height = logical_height,
        .logical_width = logical_width,
        .power_reference = power_reference,
        .min_dB = min_dB,
        .cmap = cmap,
    };
}

static float clamp_unit(float f)
{
    return fminf(fmaxf(f, 0.0f), 1.0f);
}

static Color float_to_color(float intensity, Colormap cmap, SizeType cmap_size)
{
    const float clamped = clamp_unit(intensity);
    const int index = (int)(clamped * (cmap_size - 0.0001f));
    return *(Color*)cmap[index];
}

LinearSpectrogram linear_spectrogram_new(const LinearSpectrogramConfig* cfg)
{
    Image img = GenImageColor(cfg->logical_width, cfg->logical_height, BLACK);
    Texture2D texture = LoadTextureFromImage(img);
    UnloadImage(img);
    SetTextureFilter(texture, TEXTURE_FILTER_BILINEAR);

    Color* column_buffer = malloc(sizeof(Color) * cfg->logical_height);

    return (LinearSpectrogram){
        .texture = texture,
        .column_buffer = column_buffer,
        .cfg = *cfg,
    };
}

void linear_spectrogram_destroy(LinearSpectrogram* spec)
{
    if (!spec) {
        return;
    }

    UnloadTexture(spec->texture);
    free(spec->column_buffer);
}

static Color linear_spectrogram_assign_color(const LinearSpectrogram* spec,
                                             Complex bin)
{
    const LinearSpectrogramConfig* cfg = &spec->cfg;

    const float re = crealf(bin);
    const float im = cimagf(bin);

    const float power = re * re + im * im;
    const float db = 10.0f * log10f((power / cfg->power_reference) + 1e-9f);

    // scale [min_db, reference_power] to [0, 1]
    // reference_power would appear here but it's 0dB by definition
    const float intensity = (db - cfg->min_dB) / (-cfg->min_dB);

    return float_to_color(intensity, cfg->cmap, COLORMAP_SIZE);
}

static void linear_spectrogram_update_column(LinearSpectrogram* spec,
                                             const Complex* bins,
                                             SizeType index)
{
    for (SizeType b = 0; b < spec->cfg.logical_height; b++) {
        spec->column_buffer[b] = linear_spectrogram_assign_color(spec, bins[b]);
    }

    UpdateTextureRec(
        spec->texture,
        (Rectangle){(float)index, 0, 1, (float)spec->cfg.logical_height},
        spec->column_buffer);
}

void linear_spectrogram_update(LinearSpectrogram* spec,
                               const FFTHistory* h,
                               SizeType n)
{
    // h->cap aliases spec->cfg.logical_width as the texture is the fft history
    // but on the GPU. hence how `i` indexes both the history and the texture
    n = (n >= h->cap) ? h->cap : n;
    const SizeType start = (h->tail - n + h->cap) % h->cap;

    for (SizeType i = 0; i < n; i++) {
        const SizeType index = (start + i) % h->cap;
        const Complex* bins = fft_history_get_row(h, index);
        linear_spectrogram_update_column(spec, bins, index);
    }
}

void linear_spectrogram_render_wrap(const LinearSpectrogram* spec,
                                    const FFTHistory* h)
{
    //                    REVERSED HORIZONTALLY v
    const Rectangle src = {0, 0, (float)h->len,
                           -(float)spec->cfg.logical_height};

    const Rectangle* screen = &spec->cfg.screen;
    const float screen_draw_width =
        ((float)h->len / (float)h->cap) * screen->width;
    const Rectangle dest = {screen->x, screen->y, screen_draw_width,
                            screen->height};

    DrawTexturePro(spec->texture, src, dest, (Vector2){0, 0}, 0.0f, WHITE);

    if (h->len >= h->cap) {
        const float cursor_x =
            screen->x + ((float)h->tail / h->cap) * screen->width;
        DrawLineV((Vector2){cursor_x, screen->y},
                  (Vector2){cursor_x, screen->y + screen->height}, RED);
    }
}
