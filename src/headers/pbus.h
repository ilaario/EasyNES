//
// Created by Dario Bonfiglio on 10/17/25.
//

#ifndef EASYNES_PBUS_H
#define EASYNES_PBUS_H

#include "mapper.h"
#include "cartridge.h"

struct picture_bus{
    size_t nametable0, nametable1, nametable2, nametable3;
    uint8_t* palette;
    uint8_t* ram;
    mapper mapper;

    size_t palette_size;
    size_t RAM_size;
};

typedef struct picture_bus* pbus;

pbus pbus_init(pbus pb);
void pbus_destroy(pbus pb);

uint8_t pbread(pbus p, uint16_t addr);
void pbwrite(pbus p, uint16_t addr, uint8_t value);

bool set_mapper(pbus p, mapper m);
uint8_t read_palette(pbus p, uint16_t palette_addr);
void update_mirroring(pbus p);
void scanline_IRQ(pbus p);

#endif //EASYNES_PBUS_H
