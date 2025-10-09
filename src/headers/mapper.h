//
// Created by Dario Bonfiglio on 10/9/25.
//

#ifndef EASYNES_MAPPER_H
#define EASYNES_MAPPER_H

#include <stdint.h>
#include <stdbool.h>

#include "cartridge.h"

struct Cartridge;

// vtable semplice per mapper
struct Mapper {
    // CPU space
    uint8_t (*cpu_read)(struct Mapper*, uint16_t addr);
    void    (*cpu_write)(struct Mapper*, uint16_t addr, uint8_t v);
    // PPU space (CHR)
    uint8_t (*chr_read)(struct Mapper*, uint16_t addr);
    void    (*chr_write)(struct Mapper*, uint16_t addr, uint8_t v);
    // opzionale
    void    (*reset)(struct Mapper*);

    // storage comune
    cartridge cart;
};

typedef struct Mapper* mapper;

// factory per NROM (mapper 0)
mapper mapper_nrom_create(cartridge cart);
void           mapper_destroy(mapper m);

#endif //EASYNES_MAPPER_H
