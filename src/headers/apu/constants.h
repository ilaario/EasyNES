//
// Created by Dario Bonfiglio on 10/19/25.
//

#ifndef EASYNES_CONSTANTS_H
#define EASYNES_CONSTANTS_H

#include <stdint.h>

const float    max_volume_f        = (float)0xF;
const int      max_volume          = 0xF;

const uint64_t CPU_CLOCK_PERIOD_NS = 559ULL;
const double   CPU_CLOCK_PERIOD_S  = 559e-9;

const uint64_t APU_CLOCK_PERIOD_NS = CPU_CLOCK_PERIOD_NS * 2;
const double   APU_CLOCK_PERIOD_S  = CPU_CLOCK_PERIOD_S * 2.0;

#endif //EASYNES_CONSTANTS_H
