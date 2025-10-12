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
    uint8_t (*cpu_read)(struct Mapper*, uint16_t addr);
    void    (*cpu_write)(struct Mapper*, uint16_t addr, uint8_t v);
    uint8_t (*chr_read)(struct Mapper*, uint16_t addr);
    void    (*chr_write)(struct Mapper*, uint16_t addr, uint8_t v);
    void    (*reset)(struct Mapper*);

    cartridge cart;
};

typedef struct Mapper* mapper;

// factory per NROM (mapper 0)
mapper mapper_nrom_create(cartridge cart);
void mapper_destroy(mapper m);

void mapper_nrom_attach(mapper m, cartridge c);

mapper mapper_mmc1_create(cartridge cart);  // ritorna NULL se alloc fallisce

mapper create_mapper_for_cart(cartridge cart);

void DEBUG_change_cart(mapper m, cartridge cart);

#endif //EASYNES_MAPPER_H
