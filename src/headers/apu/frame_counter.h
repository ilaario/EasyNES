//
// Created by Dario Bonfiglio on 10/19/25.
//

#ifndef EASYNES_FRAME_COUNTER_H
#define EASYNES_FRAME_COUNTER_H

#pragma once
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#include "../irq.h"

struct FrameClockable {
    void (*quarter_frame_clock)(struct FrameClockable *self);
    void (*half_frame_clock)(struct FrameClockable *self);
};

typedef struct FrameClockable* frame_clockable;


typedef enum {
    FC_Seq4Step = 0,
    FC_Seq5Step = 1
} FrameCounterMode;

enum {
    FC_Q1              = 7457,
    FC_Q2              = 14913,
    FC_Q3              = 22371,
    FC_Q4              = 29829,
    FC_preQ4           = FC_Q4 - 1,
    FC_postQ4          = FC_Q4 + 1,
    FC_seq4step_length = FC_postQ4,

    FC_Q5              = 37281,
    FC_seq5step_length = FC_Q5 + 1
};

struct FrameCounter {
    frame_clockable   *frame_slots;
    size_t             slots_count;

    FrameCounterMode   mode;
    int                counter;
    bool               interrupt_inhibit;

    irq_handle         irq;
    bool               frame_interrupt;
};

typedef struct FrameCounter* frame_counter;

void frame_counter_init(frame_counter fc,
                        frame_clockable *slots, size_t slots_count,
                        irq_handle irq);

void frame_counter_clear_frame_interrupt(frame_counter fc);
void frame_counter_reset(frame_counter fc, FrameCounterMode mode, bool irq_inhibit);
void frame_counter_clock(frame_counter fc);


#endif //EASYNES_FRAME_COUNTER_H
