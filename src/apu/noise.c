//
// Created by Dario Bonfiglio on 10/19/25.
//

#include "../headers/apu/noise.h"

void noise_init(noise n){
    if(!n) exit(EXIT_FAILURE);
    n -> volume = (volume)calloc(1, sizeof(struct Volume));
    volume_init(n -> volume);
    n -> length = (length_counter)calloc(1, sizeof(struct LengthCounter));
    length_init(n -> length);
    n -> divider = (divider)calloc(1, sizeof(struct Divider));
    divider_init(n -> divider, 0);

    n -> mode            = Bit1;
    n -> period         = 0;
    n -> shift_reg      = 1;

}

void set_period_from_table(noise n, int idx){
    const static int periods[] = {
            4, 8, 16, 32,
            64,96, 128, 160,
            202, 254, 380, 508,
            762, 1016, 2034, 4068,
    };

    set_period(n -> divider, periods[idx]);
}

void n_clock(noise n){
    if(!div_clock(n -> divider)) return;

    bool feedback_input1 = (n -> shift_reg & 0x2) ? mode == Bit1 : (n -> shift_reg & 0x40);
    bool feedback_input2 = (n -> shift_reg & 0x1);

    bool feedback        = feedback_input1 != feedback_input2;

    n -> shift_reg       = n -> shift_reg >> 1 | (feedback << 14);
}

uint8_t n_sample(noise n){
    return get(n -> volume);
}