//
// Created by Dario Bonfiglio on 10/19/25.
//

#ifndef EASYNES_UNITS_H
#define EASYNES_UNITS_H

#include "constants.h"
#include "divider.h"
#include "frame_counter.h"

struct LengthCounter{
    struct FrameClockable base;
    bool                  halt;
    bool                  enabled;
    int                   counter;
};

typedef struct LengthCounter* length_counter;

void length_init(length_counter l);

void set_enable(length_counter f, bool new_value);
bool is_enabled(length_counter f);

void set_from_table(length_counter f, size_t index);
void half_frame_clock(length_counter f);
bool muted(length_counter f);

struct LinearCounter{
    struct FrameClockable base;
    bool                  reload;
    int                   reload_value;
    bool                  control;
    int                   counter;
};

typedef struct LinearCounter* linear_counter;

void linear_init(linear_counter l);

void set_linear(linear_counter f, int new_value);
void lc_quarter_frame_clock(linear_counter f);

struct Volume{
    struct FrameClockable base;
    uint32_t              fixedVolumeOrPeriod;
    uint32_t              decayVolume;
    divider               divider;
    bool                  constant_volume;
    bool                  is_looping;
    bool                  should_start;
};

typedef struct Volume* volume;

void volume_init(volume v);

void v_quarter_frame_clock(volume f);
int  get(volume f);

#endif //EASYNES_UNITS_H
