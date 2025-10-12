//
// Created by Dario Bonfiglio on 10/11/25.
//


#include "headers/mapper.h"

static inline uint8_t safe_ff_if_null(const void *p) { return p ? 0 : 0xFF; }

struct MapperNROM {
    // vtable (stessa firma di headers/mapper.h)
    struct Mapper base;

    // stato derivato
    uint32_t prg_bank0_base;
    uint32_t prg_bank1_base;
};

typedef struct MapperNROM* mapperNROM;

mapperNROM as_nrom(mapper m){
    return (mapperNROM)m;
}

void nrom_reset(mapper m){
    mapperNROM n = as_nrom(m);
    uint16_t banks16 = n -> base.cart -> header.prg_rom_size_bytes / (KIB(16));
    if(banks16 == 1) {
        n -> prg_bank0_base = 0;
        n -> prg_bank1_base = 0;
    } else {
        n -> prg_bank0_base = 0;
        n -> prg_bank1_base = KIB(16);
    }
}

uint8_t nrom_cpu_read(mapper m, uint16_t addr){
    if (!m) return 0xFF;
    mapperNROM n = as_nrom(m);
    if (!n || !n->base.cart) return 0xFF;

    cartridge c = n->base.cart;

    // $6000–$7FFF : PRG-RAM (se presente)
    if (addr < 0x6000) return 0xFF;
    if (addr < 0x8000) {
        if (n->base.cart->prg_ram) {
            return n->base.cart->prg_ram[(size_t)(addr - 0x6000)];
        }
        return 0xFF;
    }

    // $8000–$FFFF : PRG-ROM (NROM-128: 16KiB mirror; NROM-256: 32KiB lineare)
    // ATTENZIONE: prg_rom_size_bytes nel tuo header è in KiB.
    size_t prg_size_bytes = (size_t)KIB(n->base.cart->header.prg_rom_size_bytes); // 16384 o 32768
    size_t mask           = prg_size_bytes - 1;                                      // 0x3FFF o 0x7FFF
    size_t index          = ((size_t)(addr - 0x8000)) & mask;                        // mirror se 16KiB

    return n->base.cart->prg_rom[index];
}

void nrom_cpu_write(mapper m, uint16_t addr, uint8_t v){
    if (!m) return;
    mapperNROM n = as_nrom(m);
    if (!n || !n->base.cart) return;

    if((addr >= 0x6000 && addr <= 0x7FFF) && n -> base.cart -> prg_ram != NULL){
        n -> base.cart -> prg_ram[addr - 0x6000] = v;
    }
    // else no-op
}

uint8_t nrom_chr_read(mapper m, uint16_t addr){
    mapperNROM n = as_nrom(m);
    uint16_t a = addr & 0x1FFF;
    if(n -> base.cart -> chr_rom == NULL){
        size_t size = (size_t) KIB(8);
        size_t mask = size - 1;
        return n -> base.cart -> chr_ram[a & mask];
    }
    size_t size = (size_t) KIB(n -> base.cart -> header.chr_rom_size_bytes);
    size_t mask = size - 1;
    return n -> base.cart -> chr_rom[a & mask];
}

void nrom_chr_write(mapper m, uint16_t addr, uint8_t v){
    mapperNROM n = as_nrom(m);
    uint16_t a = addr & 0x1FFF;
    if(n -> base.cart -> chr_rom == NULL){
        size_t size = (size_t) KIB(8);
        size_t mask = size - 1;
        n -> base.cart -> chr_ram[a & mask] = v;
    }
    // else no-op
}

mapper mapper_nrom_create(cartridge cart){
    mapperNROM n = (mapperNROM)malloc(sizeof(struct MapperNROM));
    if(n == NULL){
        exit(EXIT_FAILURE);
    }

    n -> base.cpu_read = nrom_cpu_read;
    n -> base.cpu_write = nrom_cpu_write;
    n -> base.chr_read = nrom_chr_read;
    n -> base.chr_write = nrom_chr_write;
    n -> base.reset = nrom_reset;

    n -> base.cart = cart;

    n -> base.reset((mapper)n);
    return (mapper)n;
}

static void nrom_configure_banks(struct MapperNROM *nm) {
    cartridge c = nm->base.cart;
    if (!c || !c->prg_rom) {
        nm->prg_bank0_base = nm->prg_bank1_base = 0;
        return;
    }
    if (c->header.prg_rom_size_bytes == 16) {           // NROM-128
        nm->prg_bank0_base = 0;
        nm->prg_bank1_base = 0;                        // mirror
    } else {                                      // NROM-256
        nm->prg_bank0_base = 0;
        nm->prg_bank1_base = 16 * 1024;
    }
}

void mapper_destroy(mapper m){
    free(as_nrom(m));
}

void mapper_nrom_attach(mapper m, cartridge c) {
    struct MapperNROM *nm = (struct MapperNROM *)m;
    nm -> base.cart = c;
    nrom_configure_banks(nm);
}

