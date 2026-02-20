//
// Created by Dario Bonfiglio on 10/17/25.
//

#include "headers/pbus.h"

pbus pbus_init(pbus pb){
    if (!pb) return NULL;
    pb -> mapper = NULL;
    pb -> RAM_size = 0x800;
    pb -> ram = (uint8_t*)calloc(pb -> RAM_size, sizeof(uint8_t));
    pb -> palette_size = 0x20;
    pb -> palette = (uint8_t*)calloc(pb -> palette_size, sizeof(uint8_t));
    pb -> nametable0 = pb -> nametable1 = pb -> nametable2 = pb -> nametable3 = 0;
    return pb;
}

void pbus_destroy(pbus pb) {
    if (!pb) return;
    free(pb -> palette);
    free(pb -> ram);
    pb -> palette = NULL;
    pb -> ram = NULL;
}

uint8_t pbread(pbus p, uint16_t addr){
    if (!p) return 0;
    update_mirroring(p);
    addr = addr & 0x3FFF;
    if(addr < 0x2000) {
        if (!p -> mapper || !p -> mapper -> chr_read) return 0;
        return p -> mapper -> chr_read(p -> mapper, addr);
    }
    else if(addr <= 0x3EFF){
        const uint16_t index = addr & 0x3FF;
        uint16_t normalised_addr = (addr >= 0x3000) ? addr - 0x1000 : addr;

        if(p -> nametable0 >= p -> RAM_size) {
            if (!p -> mapper || !p -> mapper -> chr_read) return 0;
            return p -> mapper -> chr_read(p -> mapper, normalised_addr);
        }
        else if(normalised_addr < 0x2400) return p -> ram[p -> nametable0 + index];
        else if(normalised_addr < 0x2800) return p -> ram[p -> nametable1 + index];
        else if(normalised_addr < 0x2C00) return p -> ram[p -> nametable2 + index];
        else return p -> ram[p -> nametable3 + index];
    } else if (addr <= 0x3FFF){
        uint16_t palette_addr = addr & 0x1F;
        return read_palette(p, palette_addr);
    }
    return 0x00;
}

void pbwrite(pbus p, uint16_t addr, uint8_t value){
    if (!p) return;
    update_mirroring(p);
    addr = addr & 0x3FFF;
    if(addr < 0x2000) {
        if (p -> mapper && p -> mapper -> chr_write) p -> mapper -> chr_write(p -> mapper, addr, value);
    }
    else if(addr <= 0x3EFF){
        const uint16_t index = addr & 0x3FF;
        uint16_t normalised_addr = (addr >= 0x3000) ? addr - 0x1000 : addr;

        if(p -> nametable0 >= p -> RAM_size) {
            if (p -> mapper && p -> mapper -> chr_write) p -> mapper -> chr_write(p -> mapper, normalised_addr, value);
        }
        else if(normalised_addr < 0x2400) p -> ram[p -> nametable0 + index] = value;
        else if(normalised_addr < 0x2800) p -> ram[p -> nametable1 + index] = value;
        else if(normalised_addr < 0x2C00) p -> ram[p -> nametable2 + index] = value;
        else p -> ram[p -> nametable3 + index] = value;
    } else if (addr <= 0x3FFF){
        uint16_t palette_addr = addr & 0x1F;
        if (palette_addr >= 0x10 && palette_addr % 4 == 0) palette_addr &= 0xF;
        p -> palette[palette_addr]  = (uint8_t)(value & 0x3F);
    }
}

bool set_mapper(pbus p, mapper m){
    if(!m){
        perror("Mapper is NULL");
        return false;
    }

    p -> mapper = m;
    update_mirroring(p);
    return true;
}

uint8_t read_palette(pbus p, uint16_t palette_addr){
    if (palette_addr >= 0x10 && palette_addr % 4 == 0) palette_addr &= 0xF;
    return (uint8_t)(p -> palette[palette_addr] & 0x3F);
}

void update_mirroring(pbus p){
    if (!p || !p -> mapper || !p -> mapper -> get_mirror_type) return;
    switch (p -> mapper -> get_mirror_type(p -> mapper)) {
        case MIRROR_VERTICAL:
            p -> nametable0 = p -> nametable2 = 0;
            p -> nametable1 = p -> nametable3 = 0x400;
            break;
        case MIRROR_HORIZONTAL:
            p -> nametable0 = p -> nametable1 = 0;
            p -> nametable2 = p -> nametable3 = 0x400;
            break;
        case ONE_LOWER_SCREEN:
            p -> nametable0 = p -> nametable1 = p -> nametable2 = p -> nametable3 = 0;
            break;
        case ONE_SCREEN_HIGHER:
            p -> nametable0 = p -> nametable1 = p -> nametable2 = p -> nametable3 = 0x400;
            break;
        case FOUR_SCREEN:
            p -> nametable0 = p -> RAM_size;
            break;
        default:
            p -> nametable0 = p -> nametable1 = p -> nametable2 = p -> nametable3 = 0;
            perror("Unsupported Name Table Mirroring -> %d", p -> mapper -> get_mirror_type(p -> mapper));
    }
}

void scanline_IRQ(pbus p){
    if (p && p -> mapper && p -> mapper -> scanlineIRQ) {
        p -> mapper -> scanlineIRQ(p -> mapper);
    }
}
