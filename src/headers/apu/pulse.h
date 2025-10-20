//
// Created by Dario Bonfiglio on 10/19/25.
//

#ifndef EASYNES_PULSE_H
#define EASYNES_PULSE_H

#include "divider.h"
#include "pulse_units.h"
#include "units.h"
#include "../cartridge.h"

enum Pulse_Type {
    Pulse1 = 1,
    Pulse2 = 2,
} type;

struct Pulse{
    volume            volume;
    length_counter    length_counter;

    size_t            seq_idx;
    enum Pulse_U_Type seq_type;
    divider           sequencer;
    int               period;
    enum Pulse_Type   type;
    sweep             sweep;
};

typedef struct Pulse* pulse;

void    pulse_init(pulse p, enum Pulse_Type t);

void    p_set_period(pulse p, int pi);
void    p_clock(pulse p);
uint8_t p_sample(pulse p);

#endif //EASYNES_PULSE_H
