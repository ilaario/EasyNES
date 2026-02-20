//
// Created by Dario Bonfiglio on 10/19/25.
//

#include "../headers/apu/timer.h"

void timer_init(timer t, uint64_t period_ns) {
    if (!t) return;
    t -> period = period_ns;
    t -> leftover = 0;
}

int timer_clock(timer t, uint64_t elapsed_ns) {
    if (!t || t -> period == 0) return 0;

    t -> leftover += elapsed_ns;
    if (t -> leftover < t -> period) return 0;

    int ticks = (int)(t -> leftover / t -> period);
    t -> leftover %= t -> period;
    return ticks;
}
