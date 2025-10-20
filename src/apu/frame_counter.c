//
// Created by Dario Bonfiglio on 10/20/25.
//

#include "../headers/apu/frame_counter.h"


void frame_counter_init(frame_counter fc,
                        frame_clockable *slots, size_t slots_count,
                        irq_handle irq){
    fc -> frame_slots       = slots;
    fc -> slots_count       = slots_count;
    fc -> irq               = irq;
    fc -> mode              = FC_Seq4Step;
    fc -> counter           = 0;
    fc -> interrupt_inhibit = false;
    fc -> frame_interrupt   = false;
}

void frame_counter_clear_frame_interrupt(frame_counter fc){
    if(fc -> frame_interrupt){
        fc -> frame_interrupt = false;
        fc -> irq -> release(fc -> irq);
    }
}

void frame_counter_reset(frame_counter fc, FrameCounterMode mode, bool irq_inhibit){
    fc -> mode = mode;
    fc -> interrupt_inhibit = irq_inhibit;

    if(fc -> interrupt_inhibit) frame_counter_clear_frame_interrupt(fc);

    if(fc -> mode == FC_Seq5Step){
        for(int i = 0; i < fc -> slots_count; i++){
            fc -> frame_slots[i] -> quarter_frame_clock(fc -> frame_slots[i]);
            fc -> frame_slots[i] -> half_frame_clock(fc -> frame_slots[i]);
        }
    }
}

void frame_counter_clock(frame_counter fc){
    fc -> counter += 1;

    switch (fc -> counter) {
        case FC_Q1:
            for(int i = 0; i < fc -> slots_count; i++){
                fc -> frame_slots[i] -> quarter_frame_clock(fc -> frame_slots[i]);
            }
            break;
        case FC_Q2:
            for(int i = 0; i < fc -> slots_count; i++){
                fc -> frame_slots[i] -> quarter_frame_clock(fc -> frame_slots[i]);
                fc -> frame_slots[i] -> half_frame_clock(fc -> frame_slots[i]);
            }
            break;
        case FC_Q3:
            for(int i = 0; i < fc -> slots_count; i++){
                fc -> frame_slots[i] -> quarter_frame_clock(fc -> frame_slots[i]);
            }
            break;
        case FC_Q4:
            if(fc -> mode != FC_Seq4Step) break;
            for(int i = 0; i < fc -> slots_count; i++){
                fc -> frame_slots[i] -> quarter_frame_clock(fc -> frame_slots[i]);
                fc -> frame_slots[i] -> half_frame_clock(fc -> frame_slots[i]);
            }
            if(!fc -> interrupt_inhibit){
                fc -> irq -> pull(fc -> irq);
                fc -> frame_interrupt = true;
            }
            break;
        case FC_Q5:
            if(fc -> mode != FC_Seq5Step) break;
            for(int i = 0; i < fc -> slots_count; i++){
                fc -> frame_slots[i] -> quarter_frame_clock(fc -> frame_slots[i]);
                fc -> frame_slots[i] -> half_frame_clock(fc -> frame_slots[i]);
            }
            break;
        default:
            break;
    }

    if((fc -> mode == FC_Seq4Step && fc -> counter == FC_seq4step_length) || (fc -> counter == FC_seq5step_length)) fc -> counter = 0;
}