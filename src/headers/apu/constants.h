//
// Created by Dario Bonfiglio on 10/19/25.
//

#ifndef EASYNES_CONSTANTS_H
#define EASYNES_CONSTANTS_H

#include <stdint.h>

static const float    max_volume_f        = (float)0xF;
static const int      max_volume          = 0xF;

static const uint64_t CPU_CLOCK_PERIOD_NS = 559ULL;
static const double   CPU_CLOCK_PERIOD_S  = 559e-9;

static const uint64_t APU_CLOCK_PERIOD_NS = CPU_CLOCK_PERIOD_NS * 2;
static const double   APU_CLOCK_PERIOD_S  = CPU_CLOCK_PERIOD_S * 2.0;

#endif //EASYNES_CONSTANTS_H
