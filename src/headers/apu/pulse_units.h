//
// Created by Dario Bonfiglio on 10/19/25.
//

#ifndef EASYNES_PULSE_UNITS_H
#define EASYNES_PULSE_UNITS_H

#include "divider.h"
#include "frame_counter.h"

struct Pulse;

enum Pulse_U_Type
{
    SEQ_12_5   = 0,
    SEQ_25     = 1,
    SEQ_50     = 2,
    SEQ_25_INV = 3,
};

const int count_pd  = 4;
const int length_pd = 8;

static inline bool active(enum Pulse_U_Type cycle, int idx)
{
    const bool _sequences[32] = {
            0, 0, 0, 0, 0, 0, 0, 1, // 12.5%
            0, 0, 0, 0, 0, 0, 1, 1, //   25%
            0, 0, 0, 0, 1, 1, 1, 1, //   50%
            1, 1, 1, 1, 1, 1, 0, 0, //   25% negated
    };
    return _sequences[(int)(cycle) * length_pd + idx];
}

struct Sweep{
    struct FrameClockable base;
    struct Pulse* pulse;

    int      period;
    bool     enabled;
    bool     reload;
    bool     negate;
    uint8_t  shift;
    bool     ones_complement;

    divider div;
};

typedef struct Sweep* sweep;

void        sweep_init(sweep s, struct Pulse* p, bool ones_complement);
void        s_half_frame_clock(sweep s);
static bool is_muted(int current, int target) { return current < 8 || target > 0x7FF; }
int         calculate_target(sweep s, int current);

#endif //EASYNES_PULSE_UNITS_H
