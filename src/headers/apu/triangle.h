//
// Created by Dario Bonfiglio on 10/19/25.
//

#ifndef EASYNES_TRIANGLE_H
#define EASYNES_TRIANGLE_H

#include "divider.h"
#include "units.h"
#include "../cartridge.h"

struct Triangle{
    length_counter length;
    linear_counter linear;

    uint32_t       seq_idx;
    divider        sequencer;
    int            period;
};

typedef struct Triangle* triangle;

void          triangle_init(triangle t);

void          t_set_period(triangle t, int p);
void          t_clock(triangle t);
uint8_t       t_sample(triangle t);
int           t_volume(triangle t);

#endif //EASYNES_TRIANGLE_H
