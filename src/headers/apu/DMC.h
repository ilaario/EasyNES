//
// Created by Dario Bonfiglio on 10/19/25.
//

#ifndef EASYNES_DMC_H
#define EASYNES_DMC_H

#include "divider.h"
#include "../cartridge.h"
#include "../irq.h"

struct CPU;
typedef struct CPU* cpu;

struct DMC{
    bool       irqEnable;
    bool       loop;
    int        volume;
    bool       change_enabled;
    divider    change_rate;
    uint16_t   sample_begin;
    int        sample_length;
    int        remaining_bytes;
    uint16_t   current_address;
    uint8_t    sample_buffer;
    bool       sample_buffer_empty;
    int        shifter;
    int        remaining_bits;
    bool       silenced;
    bool       interrupt;

    bool       dma_pending;
    bool       dma_reload_type;
    uint16_t   dma_address;
    uint16_t   dma_halt_address;
    bool       dma_halt_on_write;

    irq_handle irq;
    cpu       dma_cpu;
    uint8_t (*dma)(cpu, uint16_t, uint16_t, bool, int);
};

typedef struct DMC* dmc;

void    dmc_init(dmc d, irq_handle h, cpu c, uint8_t (*dma)(cpu, uint16_t, uint16_t, bool, int));
void    set_irq_enable(dmc d, bool enable);
void    set_rate(dmc d, int idx);
void    div_control(dmc d, bool enable);
void    clear_interrupt(dmc d);

void    dmc_clock(dmc d);
uint8_t dmc_sample(dmc d);
static inline bool has_more_samples(dmc d) { return d -> remaining_bytes > 0; }

#endif //EASYNES_DMC_H
