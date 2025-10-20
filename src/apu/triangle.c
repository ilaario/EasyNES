//
// Created by Dario Bonfiglio on 10/19/25.
//

#include "../headers/apu/triangle.h"


static inline int calc_note_freq(int period, int seq_length, double clock_period_ns) {
    return (int)(1e9 / (clock_period_ns * period * seq_length));
}

void triangle_init(triangle t){
    if(!t) exit(EXIT_FAILURE);
    t -> length = (length_counter)calloc(1, sizeof(struct LengthCounter));
    length_init(t -> length);
    t -> linear = (linear_counter)calloc(1, sizeof(struct LinearCounter));
    linear_init(t -> linear);

    t -> seq_idx = 0;
    t -> sequencer = (divider)calloc(1, sizeof(struct Divider));
    divider_init(t -> sequencer, 0);
    t ->period = 0;
}

void t_set_period(triangle t, int p){
    t -> period = p;
    set_period(t -> sequencer, p);
}

void t_clock(triangle t){
    if(muted(t -> length)) return;
    if(t -> linear -> counter == 0) return;
    if(clock(t -> sequencer)) t -> seq_idx = (t -> seq_idx + 1) % 32;
}

uint8_t t_sample(triangle t){
    return t_volume(t);
}

int t_volume(triangle t){
    if (t -> seq_idx < 16) return (15 - t -> seq_idx);
    else return (t -> seq_idx - 16);
}