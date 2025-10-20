//
// Created by Dario Bonfiglio on 10/19/25.
//

#include "../headers/apu/DMC.h"

void dmc_init(dmc d, irq_handle h, uint8_t (*dma)(uint16_t)){
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
    d -> shifter         = 0;
    d -> remaining_bits  = 0;
    d -> silenced        = false;
    d -> interrupt       = false;

    d -> irq = h;
    d -> dma = dma;
}

void set_irq_enable(dmc d, bool enable){
    d -> irqEnable = enable;
    if(!enable) d -> remaining_bytes = 0;
    else if (d -> remaining_bytes == 0){
        d -> current_address = d -> sample_begin;
        d -> remaining_bytes = d -> sample_length;
    }
}

void set_rate(dmc d, int idx){
    const static int rate[] = { 428, 380, 340, 320, 286, 254, 226, 214,
                                190, 160, 142, 128, 106, 84, 72, 54 };

    set_period(d -> change_rate, rate[idx]);
    reset(d -> change_rate);
}

void control(dmc d, bool enable);

void clear_interrupt(dmc d){
    d -> irq -> release(d -> irq);
    d -> interrupt = false;
}

bool load_sample(dmc d){
    if(d -> remaining_bytes == 0){
        if(!d -> loop){
            if(d -> irqEnable){
                d -> interrupt = true;
                d -> irq -> pull(d -> irq);
            }

            return false;
        }

        d -> current_address = d -> sample_begin;
        d -> remaining_bytes = d -> sample_length;
    }else{
        d -> remaining_bytes -= 1;
    }

    d -> sample_buffer = d -> dma(d -> current_address);

    if(d -> current_address == 0xFFFF) d -> current_address == 0x8000;
    else d -> current_address += 1;
    return true;
}

int pop_delta(dmc d){
    if(d -> remaining_bytes == 0){
        d -> remaining_bits = 8;
        if(load_sample(d)){
            d -> shifter = d -> sample_buffer;
            d -> silenced = false;
        }else{
            d -> silenced = true;
        }
    }else{
        --d -> remaining_bits;
    }

    int rv = d -> shifter & 0x1;
    d -> shifter >>= 1;
    return rv;
}

void dmc_clock(dmc d){
    if(!d -> change_enabled) return;
    if(!clock(d -> change_rate)) return;
    int delta = pop_delta(d);
    if(d -> silenced) return;

    if(delta == 1 && d -> volume <= 125) d -> volume += 2;
    else if (delta == 0 && d -> volume >= 2) d -> volume -= 2;
}

uint8_t dmc_sample(dmc d) {
    return d -> volume;
}