// SPDX-License-Identifier: GPL-3.0-or-later
#include <stdio.h>
#include <stdlib.h>

#include <raylib.h>

#include "RMSAnalyzer.h"
#include "RMSVisualizer.h"
#include "audio_callback.h"
#include "core/LockFreeQueue.h"

#define WINDOW_WIDTH 1600
#define WINDOW_HEIGHT 900

#define ALERT_FRACTION 16
// if ALERT_FRACTION is 10, alert at 90% fullness
#define ALMOSTFULL_ALERT \
    ((ALERT_FRACTION - 1) * CLF_QUEUE_SIZE / ALERT_FRACTION)

int main(int ac, const char** av)
{
    if (ac != 2) {
        printf("Usage: spectre [audio_file]\n");
        exit(1);
    }
    const char* music_path = av[1];

    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "title");
    InitAudioDevice();

    LockFreeQueue* sample_queue = malloc(sizeof(*sample_queue));
    if (sample_queue == NULL) {
        printf("oom\n");
        exit(1);
    }
    clfq_new(sample_queue);

    LockFreeQueueProducer sample_tx = clfq_producer(sample_queue);
    init_audio_processor(&sample_tx);
    AttachAudioMixedProcessor(pull_samples_from_audio_thread);

    // analyzer
    LockFreeQueueConsumer sample_rx = clfq_consumer(sample_queue);
    RMSAnalyzer rms_analyzer = rms_analyzer_new(sample_rx);

    // visualizer
    RMSVisualizer visualizer = rms_vis_new(
        HISTORY_SIZE, WINDOW_WIDTH, WINDOW_HEIGHT, (Vector2){0.0f, 0.0f});

    Music music = LoadMusicStream(music_path);
    if (!IsMusicValid(music)) {
        printf("Failed to open %s\n", music_path);
        exit(1);
    }
    PlayMusicStream(music);

    SetTargetFPS(60);

    SizeType frame_counter = 0;
    while (!WindowShouldClose()) {
        UpdateMusicStream(music);

        const SizeType available = clfq_consumer_size_eager(&sample_rx);
        if (available > ALMOSTFULL_ALERT) {
            printf("frame %u buffer almost full: %u\n", frame_counter,
                   available);
        }

        SizeType processed = rms_analyzer_update(&rms_analyzer);
        rms_vis_update(&visualizer, &rms_analyzer.history, processed);

        {
            BeginDrawing();
            ClearBackground(BLACK);
            rms_vis_render_wrap(&visualizer, &rms_analyzer.history);
            EndDrawing();
        }

        frame_counter++;
    }

    free(sample_queue);
    rms_analyzer_destroy(&rms_analyzer);
    deinit_audio_processor();
    UnloadMusicStream(music);
    CloseAudioDevice();
    CloseWindow();
}
