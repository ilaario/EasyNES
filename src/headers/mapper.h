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
    void    (*destroy)(struct Mapper*);

    enum mirror_type (*get_mirror_type)(struct Mapper*);

    void    (*scanlineIRQ)(struct Mapper*);

    cartridge cart;
    mapper_type m_type;
};

typedef struct Mapper* mapper;

#include "irq.h"

void create_mapper(mapper* out, cartridge cart,
                   irq_handle irq
                    /*void (*mirroring_cb)(void)*/);

mapper mapper_nrom_create(cartridge cart);
mapper mapper_mmc1_create(cartridge cart);
mapper mapper_uxrom_create(cartridge cart);
mapper mapper_cnrom_create(cartridge cart);
mapper mapper_mmc3_create(cartridge cart, irq_handle irq);
mapper mapper_axrom_create(cartridge cart);
mapper mapper_colordreams_create(cartridge cart);
mapper mapper_gxrom_create(cartridge cart);
void   mapper_destroy(mapper m);

static inline bool hasExtendedRAM(mapper m) {
    return m != NULL && m -> cart != NULL && m -> cart -> prg_ram != NULL;
}


#endif //EASYNES_MAPPER_H
