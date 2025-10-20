//
// Created by Dario Bonfiglio on 10/11/25.
//

#include "headers/mapper_nrom.h"

uint8_t cpu_read(mapper m, uint16_t addr){
    nrom n = (nrom)m;
    if(!n -> one_bank) return n -> base.cart -> prg_rom[addr - 0x8000];
    else return n -> base.cart -> prg_rom[(addr - 0x8000) & 0x3FFF];
}

void cpu_write(mapper m, uint16_t addr, uint8_t v){
    perror("ROM memory write to attempt at 0x%04X to set %d", addr, v);
}

uint8_t chr_read(mapper m, uint16_t addr) {
    nrom n = (nrom)m;
    if(n -> uses_character_ram) return n -> character_ram[addr];
    else return n -> base.cart -> chr_rom[addr];
}

void chr_write(mapper m, uint16_t addr, uint8_t v){
    nrom n = (nrom)m;
    if(n -> uses_character_ram) n -> character_ram[addr] = v;
    else perror("Read-only CHR memory write to attempt at 0x%04X to set %d", addr, v);
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
        n -> character_ram = (uint8_t*) calloc(0x2000, sizeof(uint8_t));
    } else n -> uses_character_ram = false;

    n -> base.chr_read = chr_read;
    n -> base.chr_write = chr_write;
    n -> base.cpu_read = cpu_read;
    n -> base.cpu_write = cpu_write;


    return (mapper)n;
}

void mapper_destroy(mapper m){

}