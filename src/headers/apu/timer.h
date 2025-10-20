//
// Created by Dario Bonfiglio on 10/19/25.
//

#ifndef EASYNES_TIMER_H
#define EASYNES_TIMER_H

#pragma once
#include <stdint.h>

struct Timer {
    uint64_t period;    // periodo del timer in nanosecondi
    uint64_t leftover;  // tempo accumulato
};

typedef struct Timer* timer;

void timer_init(timer t, uint64_t period_ns);
int  timer_clock(timer t, uint64_t elapsed_ns);

#endif //EASYNES_TIMER_H
