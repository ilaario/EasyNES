//
// Created by Dario Bonfiglio on 10/19/25.
//

#ifndef EASYNES_AUDIO_PLAYER_H
#define EASYNES_AUDIO_PLAYER_H

#include <stdint.h>
#include <stdbool.h>
#include "../../vendor/miniaudio/miniaudio.h"

#include "apu/spsc.h"

const uint64_t callback_period_ms = 120;  // 120 ms

struct CallbackData{
    spsc_ring    ring_buffer;
    ma_resampler resampler;
    float*       input_frames_buffer;
    bool         mute;
    int          remaining_buffer_rounds;
};

const int output_sample_rate = ma_standard_sample_rate_44100;

struct AudioPlayer{
    const int           input_sample_rate;
    spsc_ring           audio_queue;

    struct CallbackData cb_data;
    bool                initialised;
    ma_device_config    device_config;
    ma_device           device;
    ma_resampler        resampler;
};

typedef struct AudioPlayer* audio_player;

void init_audio(audio_player a, int input_rate);
bool start(audio_player a);
void audio_mute(audio_player a);

#endif //EASYNES_AUDIO_PLAYER_H
