//
// Created by Dario Bonfiglio on 10/11/25.
//

#ifndef EASYNES_CPU_H
#define EASYNES_CPU_H

#include <stdint.h>
#include <stdbool.h>

#include "bus.h"
#include "irq.h"

#define INSTRUCTION_MODE_MASK        0x3

#define OPERATION_MASK               0xE0
#define OPERATION_SHIFT              5

#define ADDR_MODE_MASK               0x1C
#define ADDR_MODE_SHIFT              2

#define BRANCH_INSTR_MASK            0x1F
#define BRANCH_INSTR_MASK_RESULT     0x10
#define BRANCH_COND_MASK             0x20
#define BRANCH_ON_FLAG_SHIFT         6

#define NMI_VECTOR                   0xFFFA
#define RESET_VECTOR                 0xFFFC
#define IRQ_VECTOR                   0xFFFE

struct IRQHandler{
    struct irq_h irq_handle;
    int bit;
    cpu c;
};

typedef struct IRQHandler* irq_handler;

struct CPU{
    int                   skip_cycles;
    int                   cycles;

    // Registers
    uint16_t              PC;
    uint8_t               SP;
    uint8_t               A;
    uint8_t               X;
    uint8_t               Y;

    bool                  C;
    bool                  Z;
    bool                  I;
    bool                  D;
    bool                  V;
    bool                  N;

    bool                  pending_NMI;

    bus                   bus;

    // Each bit is assigned to an IRQ handler.
    // If any bits are set, it means the irq must be triggered
    int                   irq_pulldowns;
    irq_handler*          irq_handlers;
    int                   irq_handlers_size;
    int                   irq_handlers_capacity;
};

typedef struct CPU* cpu;

// IRQ Handler methods
void       irq_init(irq_handler irq, int bit, cpu c);
void       release(irq_handle irq);
void       pull(irq_handle irq);

bool       is_pending_IRQ(cpu c) { return !c -> I && c -> irq_pulldowns != 0; };

void       cpu_init(cpu c, bus b);
void       cpu_step(cpu c);
void       cpu_reset(cpu c);
void       cpu_addr_reset(cpu c,uint16_t start_addr);
uint16_t   get_PC(cpu c) { return c -> PC; }
void       skip_OAM_DMA_cycles(cpu c);
void       skip_DMC_DMA_cycles(cpu c);
void       nmi_interrupt(cpu c);
irq_handle create_IRQ_handler(cpu c);
void       set_IRQ_pulldown(cpu c,int bit, bool state);


#endif //EASYNES_CPU_H
