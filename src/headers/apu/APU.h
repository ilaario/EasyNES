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
    spsc_ring     audio_queue;
    timer         sampling_timer;
};

typedef struct APU* apu;

void    apu_init(apu a, audio_player player, irq_handle irq, uint8_t(*dmcDma)(cpu c, uint16_t));
void    apu_step(apu a);
void    write_register(apu a, uint16_t addr, uint8_t value);
uint8_t read_status(apu a);

#endif //EASYNES_APU_H
