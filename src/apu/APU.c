//
// Created by Dario Bonfiglio on 10/19/25.
//

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include "../headers/apu/APU.h"              // definisci qui struct APU, Pulse, Triangle, Noise, DMC, ecc.
#include "../headers/apu/frame_counter.h"    // header C che abbiamo fatto prima
#include "../headers/apu/spsc.h"             // SPSC generico (elem_size = sizeof(float))
#include "../headers/cartridge.h"

/* -------------------- Registri APU -------------------- */
enum Register {
    APU_SQ1_VOL       = 0x4000,
    APU_SQ1_SWEEP     = 0x4001,
    APU_SQ1_LO        = 0x4002,
    APU_SQ1_HI        = 0x4003,

    APU_SQ2_VOL       = 0x4004,
    APU_SQ2_SWEEP     = 0x4005,
    APU_SQ2_LO        = 0x4006,
    APU_SQ2_HI        = 0x4007,

    APU_TRI_LINEAR    = 0x4008,
    /* 0x4009 unused */
    APU_TRI_LO        = 0x400a,
    APU_TRI_HI        = 0x400b,

    APU_NOISE_VOL     = 0x400c,
    /* 0x400d unused */
    APU_NOISE_LO      = 0x400e,
    APU_NOISE_HI      = 0x400f,

    APU_DMC_FREQ      = 0x4010,
    APU_DMC_RAW       = 0x4011,
    APU_DMC_START     = 0x4012,
    APU_DMC_LEN       = 0x4013,

    APU_CONTROL       = 0x4015,
    APU_FRAME_CONTROL = 0x4017,
};

void apu_init(apu a, audio_player player, irq_handle irq, uint8_t(*dmcDma)(uint16_t)){
    a -> pulse1   = (pulse)calloc(1, sizeof(struct Pulse));
    pulse_init(a -> pulse1, Pulse1);

    a -> pulse2   = (pulse)calloc(1, sizeof(struct Pulse));
    pulse_init(a -> pulse2, Pulse2);

    a -> dmc      = (dmc)calloc(1, sizeof(struct DMC));
    dmc_init(a -> dmc, irq, dmcDma);

    a -> triangle = (triangle)calloc(1, sizeof(struct Triangle));
    triangle_init(a -> triangle);

    a -> noise    = (noise)calloc(1, sizeof(struct Noise));
    noise_init(a -> noise);

    a -> frame_counter = (frame_counter)calloc(1, sizeof(struct FrameCounter));
    frame_counter_init(a -> frame_counter, ());

    a -> sampling_timer = (timer)calloc(1, sizeof(struct Timer));
    timer_init(a -> sampling_timer, (int64_t)(1000000000LL) / (int64_t)(output_sample_rate));

    a -> audio_queue = player -> audio_queue;

}

/* -------------------- Mixer NES -------------------- */
static inline float apu_mix(uint8_t pulse1, uint8_t pulse2, uint8_t triangle, uint8_t noise, uint8_t dmc)
{
    float p1 = (float)pulse1;   /* 0..15 */
    float p2 = (float)pulse2;   /* 0..15 */
    float tri= (float)triangle; /* 0..15 */
    float n  = (float)noise;    /* 0..15 */
    float dm = (float)dmc;      /* 0..127 */

    float pulse_out = 0.0f;
    if ((p1 + p2) != 0.0f)
        pulse_out = 95.88f / ((8128.0f / (p1 + p2)) + 100.0f);

    float tnd_out = 0.0f;
    if ((tri + n + dm) != 0.0f) {
        float tnd_sum = (tri/8227.0f) + (n/12241.0f) + (dm/22638.0f);
        tnd_out = 159.79f / (1.0f / tnd_sum + 100.0f);
    }
    return pulse_out + tnd_out;
}

/* -------------------- Step CPU → APU -------------------- */
void step(apu a)
{
    n_clock(a -> noise);
    dmc_clock(a -> dmc);
    t_clock(a -> triangle);

    if (a->divideByTwo) {
        frame_counter_clock(a->frame_counter);
        p_clock(a -> pulse1);
        p_clock(a -> pulse2);

        float s = apu_mix(p_sample(a->pulse1),
                          p_sample(a->pulse2),
                          t_sample(a->triangle),
                          n_sample(a->noise),
                          dmc_sample(a->dmc));
        (void)spsc_ring_push(&a->audio_queue, &s); /* coda audio SPSC di float */
    }
    a->divideByTwo = !a->divideByTwo;
}

/* -------------------- Scrittura registri -------------------- */
void write_register(apu a, uint16_t addr, uint8_t value)
{
    switch (addr) {
        case APU_SQ1_VOL:
            a->pulse1 -> volume -> fixedVolumeOrPeriod = value & 0x0F;
            a->pulse1 -> volume -> constant_volume     = (value & (1u<<4)) != 0;
            a->pulse1 -> volume -> is_looping = a->pulse1->length_counter->halt = (value & (1u<<5)) != 0;
            a->pulse1->seq_type         = (enum Type)(value >> 6);
            break;

        case APU_SQ1_SWEEP:
            a->pulse1 -> sweep -> enabled = (value >> 7) & 1u;
            a->pulse1 -> sweep -> period  = (value >> 4) & 0x7u;
            a->pulse1 -> sweep -> negate  = (value >> 3) & 1u;
            a->pulse1 -> sweep -> shift   = value & 0x7u;
            a->pulse1 -> sweep -> reload  = true;
            break;

        case APU_SQ1_LO: {
            int new_period = (a->pulse1 -> period & 0xFF00) | value;
            p_set_period(a->pulse1, new_period);
        } break;

        case APU_SQ1_HI: {
            int new_period = (a->pulse1 -> period & 0x00FF) | ((value & 0x7) << 8);
            set_from_table(a->pulse1 -> length_counter, (size_t)(value >> 3));
            a->pulse1 -> seq_idx = 0;
            a->pulse1 -> volume -> should_start = true;
            p_set_period(a->pulse1, new_period);
        } break;

        case APU_SQ2_VOL:
            a->pulse2 -> volume -> fixedVolumeOrPeriod = value & 0x0F;
            a->pulse2 -> volume -> constant_volume     = (value & (1u<<4)) != 0;
            a->pulse2 -> volume -> is_looping = a->pulse2 -> length_counter -> halt = (value & (1u<<5)) != 0;
            a->pulse2 -> seq_type         = (enum Type)(value >> 6);
            break;

        case APU_SQ2_SWEEP:
            a->pulse2 -> sweep -> enabled = (value >> 7) & 1u;
            a->pulse2 -> sweep -> period  = (value >> 4) & 0x7u;
            a->pulse2 -> sweep -> negate  = (value >> 3) & 1u;
            a->pulse2 -> sweep -> shift   = value & 0x7u;
            a->pulse2 -> sweep -> reload  = true;
            break;

        case APU_SQ2_LO: {
            int new_period = (a->pulse2 -> period & 0xFF00) | value;
            p_set_period(a->pulse2, new_period);
        } break;

        case APU_SQ2_HI: {
            int new_period = (a->pulse2 -> period & 0x00FF) | ((value & 0x7) << 8);
            set_from_table(a->pulse2 -> length_counter, (size_t)(value >> 3));
            a->pulse2 -> seq_idx = 0;
            p_set_period(a->pulse2, new_period);
            a->pulse2 -> volume -> should_start = true;
        } break;

        case APU_TRI_LINEAR:
            set_linear(a->triangle -> linear, value & 0x7F);
            a->triangle -> linear -> reload  = true;
            a->triangle -> linear -> control = a->triangle -> length -> halt = (value >> 7) & 1u; /* nota: bit7 */
            break;

        case APU_TRI_LO: {
            int new_period = (a->triangle -> period & 0xFF00) | value;
            t_set_period(a->triangle, new_period);
        } break;

        case APU_TRI_HI: {
            int new_period = (a->triangle -> period & 0x00FF) | ((value & 0x7) << 8);
            set_from_table(a->triangle -> length, (size_t)(value >> 3));
            t_set_period(a->triangle, new_period);
            a->triangle -> linear -> reload = true;
        } break;

        case APU_NOISE_VOL:
            a->noise -> volume -> fixedVolumeOrPeriod = value & 0x0F;
            a->noise -> volume -> constant_volume     = (value & (1u<<4)) != 0;
            a->noise -> volume -> is_looping = a->noise -> length -> halt = (value & (1u<<5)) != 0;
            break;

        case APU_NOISE_LO:
            a->noise -> mode = (enum Mode)((value >> 7) & 1u);
            set_period_from_table(a->noise, value & 0x0F);
            break;

        case APU_NOISE_HI:
            set_from_table(a->noise -> length, (size_t)(value >> 3));
            reset(a->noise -> volume -> divider);
            break;

        case APU_DMC_FREQ:
            a->dmc -> irqEnable = (value >> 7) & 1u;
            a->dmc -> loop      = (value >> 6) & 1u;
            set_rate(a->dmc, value & 0x0F);
            break;

        case APU_DMC_RAW:
            a->dmc -> volume = value & 0x7F;
            break;

        case APU_DMC_START:
            a->dmc -> sample_begin = 0xC000u | ((uint16_t)value << 6);
            break;

        case APU_DMC_LEN:
            a->dmc -> sample_length = ((uint16_t)value << 4) | 1u;
            break;

        case APU_CONTROL:
            set_enable(a->pulse1 -> length_counter,   (value & 0x01) != 0);
            set_enable(a->pulse2 -> length_counter,   (value & 0x02) != 0);
            set_enable(a->triangle -> length, (value & 0x04) != 0);
            set_enable(a->noise -> length,    (value & 0x08) != 0);
            control(a->dmc, (value & 0x10) != 0);
            break;

        case APU_FRAME_CONTROL:
            frame_counter_reset(a->frame_counter,
                                (value >> 7) ? FC_Seq5Step : FC_Seq4Step,
                                (value >> 6) & 1u);
            break;
    }
}

/* -------------------- Lettura stato -------------------- */
uint8_t read_status(apu a)
{
    bool last_frame_irq = a->frame_counter -> frame_interrupt;
    frame_counter_clear_frame_interrupt(a->frame_counter);

    bool dmc_irq = a->dmc -> interrupt;
    clear_interrupt(a->dmc);

    /* bit0..3: length counters not zero; bit4: DMC bytes remaining (negato negli originali),
       bit6: frame IRQ, bit7: DMC IRQ */
    uint8_t v = 0;
    v |= (!muted(a->pulse1 -> length_counter))   << 0;
    v |= (!muted(a->pulse2 -> length_counter))   << 1;
    v |= (!muted(a->triangle -> length)) << 2;
    v |= (!muted(a->noise -> length))    << 3;
    v |= (!has_more_samples(a->dmc))                     << 4;
    v |= (last_frame_irq ? 1u : 0u)                             << 6;
    v |= (dmc_irq ? 1u : 0u)                                    << 7;
    return v;
}

/* -------------------- Setup frame counter (slots) -------------------- */
void APU_setup_frame_counter(apu a, irq_handle irq)
{
    frame_clockable slots[] = {
            &a->pulse1 -> volume -> base,
            &a->pulse1 -> sweep -> base,
            &a->pulse1 -> length_counter -> base,

            &a->pulse2 -> volume -> base,
            &a->pulse2 -> sweep -> base,
            &a->pulse2 -> length_counter -> base,

            &a->triangle -> length -> base,
            &a->triangle -> linear -> base,

            &a->noise -> volume -> base,
            &a->noise -> length -> base,
    };
    frame_counter_init(a->frame_counter, slots, sizeof(slots)/sizeof(slots[0]), irq);
}
