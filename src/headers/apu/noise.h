//
// Created by Dario Bonfiglio on 10/19/25.
//

#ifndef EASYNES_NOISE_H
#define EASYNES_NOISE_H

#include "divider.h"
#include "units.h"

enum Mode : bool {
    Bit1 = 0,
    BIt6 = 1,
} mode;

struct Noise{
    volume         volume;
    length_counter length;
    divider        divider;
    enum Mode      mode;
    int            period;
    int            shift_reg;
};

typedef struct Noise* noise;

void    noise_init(noise n);
void    set_period_from_table(noise n, int idx);
void    n_clock(noise n);
uint8_t n_sample(noise n);

#endif //EASYNES_NOISE_H
