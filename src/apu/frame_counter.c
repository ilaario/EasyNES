//
// Created by Dario Bonfiglio on 10/20/25.
//

#include "../headers/apu/frame_counter.h"
#include <stdlib.h>
#include <string.h>

static inline void quarter_clock_all(frame_counter fc) {
    for(size_t i = 0; i < fc -> slots_count; i++){
        fc -> frame_slots[i] -> quarter_frame_clock(fc -> frame_slots[i]);
    }
}

static inline void half_clock_all(frame_counter fc) {
    for(size_t i = 0; i < fc -> slots_count; i++){
        fc -> frame_slots[i] -> half_frame_clock(fc -> frame_slots[i]);
    }
}

static inline void quarter_half_clock_all(frame_counter fc) {
    quarter_clock_all(fc);
    half_clock_all(fc);
}

static inline void set_frame_irq(frame_counter fc) {
    if (fc -> interrupt_inhibit) return;
    fc -> frame_interrupt = true;
    fc -> frame_irq_set_this_cycle = true;
    fc -> irq -> pull(fc -> irq);
}

static void apply_pending_reset(frame_counter fc) {
    fc -> reset_pending = false;
    fc -> mode = fc -> pending_mode;
    fc -> interrupt_inhibit = fc -> pending_interrupt_inhibit;
    if (fc -> interrupt_inhibit) frame_counter_clear_frame_interrupt(fc);
    fc -> counter = 0;
    if (fc -> mode == FC_Seq5Step) {
        // 5-step mode clocks quarter+half immediately at reset point.
        quarter_half_clock_all(fc);
    }
}

void frame_counter_init(frame_counter fc,
                        frame_clockable *slots, size_t slots_count,
                        irq_handle irq){
    fc -> frame_slots       = NULL;
    if (slots_count > 0) {
        fc -> frame_slots = (frame_clockable*)malloc(slots_count * sizeof(frame_clockable));
        if (!fc -> frame_slots) exit(EXIT_FAILURE);
        memcpy(fc -> frame_slots, slots, slots_count * sizeof(frame_clockable));
    }
    fc -> slots_count       = slots_count;
    fc -> irq               = irq;
    fc -> mode              = FC_Seq4Step;
    fc -> counter           = 0;
    fc -> interrupt_inhibit = false;
    fc -> reset_pending     = false;
    fc -> reset_delay       = 0;
    fc -> pending_mode      = FC_Seq4Step;
    fc -> pending_interrupt_inhibit = false;
    fc -> frame_interrupt   = false;
    fc -> frame_irq_set_this_cycle = false;
}

void frame_counter_clear_frame_interrupt(frame_counter fc){
    fc -> frame_interrupt = false;
    fc -> irq -> release(fc -> irq);
}

void frame_counter_request_reset(frame_counter fc, FrameCounterMode mode, bool irq_inhibit, bool odd_cpu_cycle){
    fc -> pending_mode = mode;
    fc -> pending_interrupt_inhibit = irq_inhibit;
    fc -> reset_pending = true;
    // $4017 write takes effect after 3 CPU cycles on odd, 4 on even.
    fc -> reset_delay = odd_cpu_cycle ? 3 : 4;
    if (irq_inhibit) frame_counter_clear_frame_interrupt(fc);
}

void frame_counter_clock(frame_counter fc){
    fc -> frame_irq_set_this_cycle = false;

    if (fc -> reset_pending && fc -> reset_delay > 0) {
        fc -> reset_delay -= 1;
        if (fc -> reset_delay == 0) apply_pending_reset(fc);
    }

    fc -> counter += 1;

    switch (fc -> counter) {
        case FC_Q1:
            quarter_clock_all(fc);
            break;
        case FC_Q2:
            quarter_half_clock_all(fc);
            break;
        case FC_Q3:
            quarter_clock_all(fc);
            break;
        case FC_Q4:
            if(fc -> mode != FC_Seq4Step) break;
            quarter_half_clock_all(fc);
            set_frame_irq(fc);
            break;
        case FC_postQ4:
            if(fc -> mode != FC_Seq4Step) break;
            // Frame IRQ stays asserted for one additional CPU cycle window.
            set_frame_irq(fc);
            break;
        case FC_Q5:
            if(fc -> mode != FC_Seq5Step) break;
            quarter_half_clock_all(fc);
            break;
        default:
            break;
    }

    if((fc -> mode == FC_Seq4Step && fc -> counter >= FC_seq4step_length) ||
       (fc -> mode == FC_Seq5Step && fc -> counter >= FC_seq5step_length)) {
        fc -> counter = 0;
    }
}
