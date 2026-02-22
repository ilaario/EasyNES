//
// Created by Dario Bonfiglio on 10/19/25.
//

#ifndef EASYNES_APU_H
#define EASYNES_APU_H

#include "DMC.h"
#include "frame_counter.h"
#include "noise.h"
#include "pulse.h"
#include "timer.h"
#include "triangle.h"
#include "spsc.h"
#include "../audio_player.h"
#include "../irq.h"

struct APU{
    pulse         pulse1;
    pulse         pulse2;
    triangle      triangle;
    noise         noise;
    dmc           dmc;

    frame_counter frame_counter;

    bool          divideByTwo;
    uint64_t      cpu_cycle_count;
    spsc_ring*    audio_queue;
    timer         sampling_timer;

    float         hp90_alpha;
    float         hp440_alpha;
    float         lp14k_alpha;

    float         hp90_x_prev;
    float         hp90_y_prev;
    float         hp440_x_prev;
    float         hp440_y_prev;
    float         lp14k_y_prev;
};

typedef struct APU* apu;

void    apu_init(apu a,
                 audio_player player,
                 irq_handle frame_irq,
                 irq_handle dmc_irq,
                 cpu dmc_cpu,
                 uint8_t(*dmcDma)(cpu, uint16_t, uint16_t, bool, int));
void    apu_step(apu a);
void    write_register(apu a, uint16_t addr, uint8_t value);
uint8_t read_status(apu a);

#endif //EASYNES_APU_H
