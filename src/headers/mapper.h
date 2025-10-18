//
// Created by Dario Bonfiglio on 10/9/25.
//

#ifndef EASYNES_MAPPER_H
#define EASYNES_MAPPER_H

#include <stdint.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>

#include "cartridge.h"

typedef enum Type {
    NROM        = 0,
    SxROM       = 1,
    UxROM       = 2,
    CNROM       = 3,
    MMC3        = 4,
    AxROM       = 7,
    ColorDreams = 11,
    GxROM       = 66,
} mapper_type;

// vtable semplice per mapper
struct Mapper {
    uint8_t (*cpu_read)(struct Mapper*, uint16_t addr);
    void    (*cpu_write)(struct Mapper*, uint16_t addr, uint8_t v);
    uint8_t (*chr_read)(struct Mapper*, uint16_t addr);
    void    (*chr_write)(struct Mapper*, uint16_t addr, uint8_t v);
    void    (*reset)(struct Mapper*);

    enum mirror_type (*get_mirror_type)();

    void    (*scanlineIRQ)();

    cartridge cart;
    mapper_type m_type;
};

typedef struct Mapper* mapper;

#include "mapper_nrom.h"
#include "irq.h"

void create_mapper(mapper m, cartridge cart,
                   mapper_type t, irq_handle irq,
                   void (*mirroring_cb)(void));


mapper mapper_mmc1_create(cartridge cart);  // ritorna NULL se alloc fallisce

mapper create_mapper_for_cart(cartridge cart);

static void mmc1_remap_prg(mapper m);
static void mmc1_remap_chr(mapper m);
static void mmc1_write_control(mapper m, uint8_t v);

bool inline hasExtendedRAM(mapper m) { return true; }


#endif //EASYNES_MAPPER_H
