//
// Created by Dario Bonfiglio on 10/19/25.
//

#include "headers/audio_player.h"
#include <string.h>

static void audio_data_callback(ma_device *pDevice, void *pOutput, const void *pInput, ma_uint32 frameCount) {
    (void)pInput;

    audio_player a = (audio_player)pDevice->pUserData;
    float *out = (float *)pOutput;
    if (!out || frameCount == 0) return;

    if (!a || !a->initialised || !a->cb_data.ring_buffer || a->cb_data.mute) {
        memset(out, 0, (size_t)frameCount * sizeof(float));
        return;
    }

    size_t popped = spsc_ring_pop(a->cb_data.ring_buffer, out, (size_t)frameCount);
    if (popped < frameCount) {
        memset(out + popped, 0, (size_t)(frameCount - popped) * sizeof(float));
    }
}

void init_audio(audio_player a, int input_rate) {
    if (!a) return;
    memset(a, 0, sizeof(*a));

    if (input_rate <= 0) input_rate = output_sample_rate;
    a -> input_sample_rate = input_rate;
    a -> initialised = spsc_ring_init(&a -> audio_queue,
                                      (size_t)(output_sample_rate * 4),
                                      sizeof(float)) != 0;
    if (!a -> initialised) return;

    a -> cb_data.ring_buffer = &a -> audio_queue;
    a -> cb_data.input_frames_buffer = NULL;
    a -> cb_data.mute = false;
    a -> cb_data.remaining_buffer_rounds = 0;

    a -> device_config = ma_device_config_init(ma_device_type_playback);
    a -> device_config.playback.format = ma_format_f32;
    a -> device_config.playback.channels = 1;
    a -> device_config.sampleRate = output_sample_rate;
    a -> device_config.dataCallback = audio_data_callback;
    a -> device_config.pUserData = a;

    if (ma_device_init(NULL, &a -> device_config, &a -> device) != MA_SUCCESS) {
        spsc_ring_free(&a -> audio_queue);
        a -> initialised = false;
    }
}

bool start(audio_player a) {
    if (!a || !a -> initialised) return false;

    ma_device_state state = ma_device_get_state(&a -> device);
    if (state == ma_device_state_started || state == ma_device_state_starting) return true;
    return ma_device_start(&a -> device) == MA_SUCCESS;
}

void audio_shutdown(audio_player a) {
    if (!a) return;
    if (a -> initialised) {
        ma_device_uninit(&a -> device);
        spsc_ring_free(&a -> audio_queue);
    }
    memset(a, 0, sizeof(*a));
}

void audio_mute(audio_player a, bool mute) {
    if (!a) return;
    a -> cb_data.mute = mute;
}
