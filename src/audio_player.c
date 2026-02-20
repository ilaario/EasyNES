//
// Created by Dario Bonfiglio on 10/19/25.
//

#include "headers/audio_player.h"
#include <string.h>

void init_audio(audio_player a, int input_rate) {
    if (!a) return;
    memset(a, 0, sizeof(*a));

    if (input_rate <= 0) input_rate = output_sample_rate;
    a -> input_sample_rate = input_rate;
    a -> initialised = spsc_ring_init(&a -> audio_queue,
                                      (size_t)(output_sample_rate * 2),
                                      sizeof(float)) != 0;
    a -> cb_data.ring_buffer = &a -> audio_queue;
    a -> cb_data.input_frames_buffer = NULL;
    a -> cb_data.mute = false;
    a -> cb_data.remaining_buffer_rounds = 0;
}

bool start(audio_player a) {
    return a && a -> initialised;
}

void audio_mute(audio_player a, bool mute) {
    if (!a) return;
    a -> cb_data.mute = mute;
}
