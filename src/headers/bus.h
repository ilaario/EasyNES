//
// Created by Dario Bonfiglio on 10/9/25.
//

#ifndef EASYNES_BUS_H
#define EASYNES_BUS_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "ppu.h"         // espone ppu_reg_read/ppu_reg_write, ppu_oam_dma_push_byte
#include "mapper.h"      // espone mapper->cpu_read/cpu_write
#include "controller.h"  // espone write_strobe/read_shift

struct CPU;
typedef struct CPU* cpu;

struct PPU;
struct Mapper;
struct Controller;

struct CPUBus {
    uint8_t ram[0x800];    // 2 KiB
    ppu ppu;
    mapper mapper;
    controller pad1;
    controller pad2;

    // DMA OAM state
    bool dma_active;
    uint16_t dma_src_page;
    uint16_t dma_index;
    bool dma_even_cycle;
};

typedef struct CPUBus* bus;

void bus_init(bus bus, ppu ppu, mapper mapper, controller pad1, controller pad2);

uint8_t  bus_cpu_read8 (bus bus, uint16_t addr);
void     bus_cpu_write8(bus, uint16_t addr, uint8_t value);

// Da chiamare nel loop CPU per applicare lo stall DMA e trasferire OAM
void start_oam_dma(bus bus, uint8_t value);
int bus_tick_dma_if_active(bus bus, /*maybe*/ uint64_t cpu_cycle);

#endif //EASYNES_BUS_H
