//
// Created by Dario Bonfiglio on 10/11/25.
//

#ifndef EASYNES_CPU_H
#define EASYNES_CPU_H

#include <stdint.h>
#include <stdbool.h>

#include "bus.h"

struct CPU{
    uint8_t A, X, Y;    // Accumulators
    uint8_t P;          // NV-BDIZC Status
    uint8_t S;          // Stack Pointer
    uint16_t PC;        // Program Counter

    uint64_t cycles;    // Cycle counter
    bool     nmi_line;  // NMI Line level
    bool     irq_line;  // IRQ Line (low activity)
    bool     stalled;   // true while OAM DMA

    bus      sysbus;    // Bus pointer
};

typedef struct CPU* cpu;

cpu  create_cpu(bus b);
void cpu_destroy(cpu c);
void cpu_reset(cpu c);
void cpu_nmi(cpu c);
void cpu_irq(cpu c);
int  cpu_step(cpu c);

#endif //EASYNES_CPU_H
