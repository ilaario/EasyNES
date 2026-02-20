//
// Created by Dario Bonfiglio on 10/19/25.
//

#include "../headers/apu/DMC.h"
#include "../headers/cpu.h"

static inline uint16_t cpu_bus_address(cpu c) {
    if (!c || !c -> bus) return c ? c -> PC : 0;
    return c -> bus -> address_bus;
}

static inline bool cpu_bus_write_cycle(cpu c) {
    return c && c -> bus && c -> bus -> address_bus_is_write;
}

static inline bool apu_put_cycle(cpu c) {
    return c && ((c -> cycles & 1) != 0);
}

static void request_sample_dma(dmc d, bool reload_type) {
    if (!d || !d -> change_enabled || !d -> sample_buffer_empty || d -> dma_pending) return;

    if (d -> remaining_bytes == 0) {
        if (d -> loop) {
            d -> current_address = d -> sample_begin;
            d -> remaining_bytes = d -> sample_length;
        } else {
            if (d -> irqEnable && !d -> interrupt) {
                d -> interrupt = true;
                d -> irq -> pull(d -> irq);
            }
            return;
        }
    }

    d -> dma_pending = true;
    d -> dma_reload_type = reload_type;
    d -> dma_address = d -> current_address;
    d -> dma_halt_on_write = cpu_bus_write_cycle(d -> dma_cpu);
    d -> dma_halt_address = d -> dma_halt_on_write && d -> dma_cpu
                                ? d -> dma_cpu -> PC
                                : cpu_bus_address(d -> dma_cpu);
}

static void service_sample_dma(dmc d) {
    if (!d || !d -> dma_pending) return;

    if (!d -> change_enabled) {
        // Explicit/implicit DMA abort still consumes one CPU cycle.
        skip_DMC_DMA_cycles(d -> dma_cpu, 1);
        d -> dma_pending = false;
        return;
    }

    bool halt_on_put = apu_put_cycle(d -> dma_cpu);
    if (d -> dma_halt_on_write) halt_on_put = !halt_on_put;
    bool needs_align = d -> dma_reload_type ? !halt_on_put : halt_on_put;
    int stall_cycles = 3 + (d -> dma_halt_on_write ? 1 : 0) + (needs_align ? 1 : 0);

    uint8_t value = d -> dma(d -> dma_cpu,
                             d -> dma_address,
                             d -> dma_halt_address,
                             needs_align,
                             stall_cycles);

    d -> dma_pending = false;
    d -> sample_buffer = value;
    d -> sample_buffer_empty = false;

    if (d -> remaining_bytes > 0) d -> remaining_bytes -= 1;
    if (d -> current_address == 0xFFFF) d -> current_address = 0x8000;
    else d -> current_address += 1;
}

void dmc_init(dmc d, irq_handle h, cpu c, uint8_t (*dma)(cpu, uint16_t, uint16_t, bool, int)){
    if(!d) exit(EXIT_FAILURE);
    d -> irqEnable       = false;
    d -> loop            = false;
    d -> volume          = 0;
    d -> change_enabled  = false;
    d -> change_rate     =(divider)calloc(1, sizeof(struct Divider));
    divider_init(d -> change_rate, 0);
    d -> sample_begin    = 0;
    d -> sample_length   = 0;
    d -> remaining_bytes = 0;
    d -> current_address = 0;
    d -> sample_buffer   = 0;
    d -> sample_buffer_empty = true;
    d -> shifter         = 0;
    d -> remaining_bits  = 0;
    d -> silenced        = false;
    d -> interrupt       = false;
    d -> dma_pending     = false;
    d -> dma_reload_type = false;
    d -> dma_address     = 0;
    d -> dma_halt_address = 0;
    d -> dma_halt_on_write = false;

    d -> irq = h;
    d -> dma_cpu = c;
    d -> dma = dma;
}

void set_irq_enable(dmc d, bool enable){
    d -> irqEnable = enable;
    if(!enable) clear_interrupt(d);
}

void set_rate(dmc d, int idx){
    const static int rate[] = { 428, 380, 340, 320, 286, 254, 226, 214,
                                190, 160, 142, 128, 106, 84, 72, 54 };

    set_period(d -> change_rate, rate[idx]);
    div_reset(d -> change_rate);
}

void div_control(dmc d, bool enable){
    d -> change_enabled = enable;
    if (!enable) {
        d -> remaining_bytes = 0;
    }
    else if (d -> remaining_bytes == 0) {
        d -> current_address = d -> sample_begin;
        d -> remaining_bytes = d -> sample_length;
        request_sample_dma(d, false);
    }
}

void clear_interrupt(dmc d){
    d -> irq -> release(d -> irq);
    d -> interrupt = false;
}

int pop_delta(dmc d){
    if(d -> remaining_bits == 0){
        d -> remaining_bits = 8;
        if(!d -> sample_buffer_empty){
            d -> shifter = d -> sample_buffer;
            d -> sample_buffer_empty = true;
            d -> silenced = false;
            request_sample_dma(d, true);
        }else{
            d -> silenced = true;
        }
    }

    int rv = d -> shifter & 0x1;
    d -> shifter >>= 1;
    if (d -> remaining_bits > 0) --d -> remaining_bits;
    return rv;
}

void dmc_clock(dmc d){
    service_sample_dma(d);
    if(!d -> change_enabled) return;
    request_sample_dma(d, true);
    if(!div_clock(d -> change_rate)) return;
    int delta = pop_delta(d);
    if(d -> silenced) return;

    if(delta == 1 && d -> volume <= 125) d -> volume += 2;
    else if (delta == 0 && d -> volume >= 2) d -> volume -= 2;
}

uint8_t dmc_sample(dmc d) {
    return d -> volume;
}
