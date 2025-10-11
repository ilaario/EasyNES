//
// Created by Dario Bonfiglio on 10/11/25.
//


#include "headers/mapper.h"

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
    mapperNROM n = as_nrom(m);

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
    mapperNROM n = as_nrom(m);
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

void mapper_destroy(mapper m){
    free(as_nrom(m));
}

void DEBUG_change_cart(mapper m, cartridge cart){
    m -> cart = cart;
}
