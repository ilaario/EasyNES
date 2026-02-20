//
// Created by Dario Bonfiglio on 10/11/25.
//

#include "headers/mapper_nrom.h"

static uint8_t nrom_cpu_read(mapper m, uint16_t addr){
    nrom n = (nrom)m;
    if(!n -> one_bank) return n -> base.cart -> prg_rom[addr - 0x8000];
    else return n -> base.cart -> prg_rom[(addr - 0x8000) & 0x3FFF];
}

static void nrom_cpu_write(mapper m, uint16_t addr, uint8_t v){
    (void)m;
    // perror("ROM memory write to attempt at 0x%04X to set %d", addr, v);
}

static uint8_t nrom_chr_read(mapper m, uint16_t addr) {
    nrom n = (nrom)m;
    if(n -> uses_character_ram) return n -> character_ram[addr];
    else return n -> base.cart -> chr_rom[addr];
}

static void nrom_chr_write(mapper m, uint16_t addr, uint8_t v){
    nrom n = (nrom)m;
    if(n -> uses_character_ram) n -> character_ram[addr] = v;
    else perror("Read-only CHR memory write to attempt at 0x%04X to set %d", addr, v);
}

static enum mirror_type nrom_get_mirror_type(mapper m) {
    return m -> cart -> header.mirroring;
}

static void nrom_scanline_irq(mapper m) {
    (void)m;
}

static void nrom_destroy(mapper m) {
    free(m);
}

mapper mapper_nrom_create(cartridge cart){
    nrom n = (nrom)malloc(sizeof(struct mapper_nrom));
    if(!n){
        perror("Error allocating NROM mapper");
        exit(EXIT_FAILURE);
    }

    n -> base.cart = cart;
    if(KIB(cart -> header.prg_rom_size_bytes) == 0x4000) n -> one_bank = true;
    else n -> one_bank = false;

    if(KIB(cart -> header.chr_rom_size_bytes) == 0) {
        n -> uses_character_ram = true;
        n -> character_ram = cart -> chr_ram;
    } else {
        n -> uses_character_ram = false;
        n -> character_ram = NULL;
    }

    n -> base.chr_read = nrom_chr_read;
    n -> base.chr_write = nrom_chr_write;
    n -> base.cpu_read = nrom_cpu_read;
    n -> base.cpu_write = nrom_cpu_write;
    n -> base.get_mirror_type = nrom_get_mirror_type;
    n -> base.scanlineIRQ = nrom_scanline_irq;
    n -> base.destroy = nrom_destroy;
    n -> base.m_type = NROM;


    return (mapper)n;
}
