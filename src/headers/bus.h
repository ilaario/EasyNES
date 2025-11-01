//
// Created by Dario Bonfiglio on 10/9/25.
//

#ifndef EASYNES_BUS_H
#define EASYNES_BUS_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "ppu.h"
#include "mapper.h"
#include "controller.h"
#include "cartridge.h"
#include "apu/APU.h"

struct CPU;
typedef struct CPU* cpu;

struct PPU;
struct Mapper;
struct Controller;

enum Register{
    PPU_CTRL = 0x2000,
    PPU_MASK,   // = 0x2001,
    PPU_STATUS, // = 0x2002,
    OAM_ADDR,   // = 0x2003,
    OAM_DATA,   // = 0x2004,
    PPU_SCROL,  // = 0x2005,
    PPU_ADDR,   // = 0x2006,
    PPU_DATA,   // = 0x2007,

    // 0x2008 - 0x3fff mirrors of 0x2000 - 0x2007

    APU_REGISTER_START     = 0x4000,
    APU_REGISTER_END       = 0x4013,

    OAM_DMA                = 0x4014,

    APU_CONTROL_AND_STATUS = 0x4015,

    JOY1                   = 0x4016,
    JOY2_AND_FRAME_CONTROL = 0x4017,
};

typedef enum Register reg;

struct CPUBus {
    uint8_t* RAM;
    uint8_t* extRAM;
    void (*dma_callback)(ppu, uint8_t*);
    mapper mapper;
    ppu ppu;
    apu apu;
    cs controller_set;
};

typedef struct CPUBus* bus;

void           bus_init(bus b, ppu p, apu a, cs c, void (*dma)(ppu, uint8_t*));
uint8_t        bus_read(bus b, uint16_t addr);
void           bus_write(bus b, uint16_t addr, uint8_t value);
bool           setMapper(bus b, mapper mapper);
const uint8_t* getPagePtr(bus b, uint8_t page);

#endif //EASYNES_BUS_H
