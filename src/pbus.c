//
// Created by Dario Bonfiglio on 10/17/25.
//

#include "headers/pbus.h"

pbus pbus_init(pbus pb){
    pb -> mapper = NULL;
    pb -> RAM_size = 0x800;
    pb -> ram = (uint8_t*)calloc(pb -> RAM_size, sizeof(uint8_t));
    pb -> palette_size = 0x20;
    pb -> palette = (uint8_t*)calloc(pb -> palette_size, sizeof(uint8_t));
}

void pbus_destroy(pbus pb) {
    free(pb -> palette);
    free(pb -> ram);
}

uint8_t pbread(pbus p, uint16_t addr){
    addr = addr & 0x3FFF;
    if(addr < 0x2000) return p -> mapper -> chr_read(p -> mapper, addr);
    else if(addr <= 0x3EFF){
        const uint8_t index = addr & 0x3FF;
        uint16_t normalised_addr = (addr >= 0x3000) ? addr - 0x1000 : addr;

        if(p -> nametable0 >= p -> RAM_size) return p -> mapper -> chr_read(p -> mapper, normalised_addr);
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
    addr = addr & 0x3FFF;
    if(addr < 0x2000) p -> mapper -> chr_write(p -> mapper, addr, value);
    else if(addr <= 0x3EFF){
        const uint8_t index = addr & 0x3FF;
        uint16_t normalised_addr = (addr >= 0x3000) ? addr - 0x1000 : addr;

        if(p -> nametable0 >= p -> RAM_size) p -> mapper -> chr_write(p -> mapper, normalised_addr, value);
        else if(normalised_addr < 0x2400) p -> ram[p -> nametable0 + index] = value;
        else if(normalised_addr < 0x2800) p -> ram[p -> nametable1 + index] = value;
        else if(normalised_addr < 0x2C00) p -> ram[p -> nametable2 + index] = value;
        else p -> ram[p -> nametable3 + index] = value;
    } else if (addr <= 0x3FFF){
        uint16_t palette_addr = addr & 0x1F;
        if (palette_addr >= 0x10 && palette_addr % 4 == 0) palette_addr &= 0xF;
        p -> palette[palette_addr]  = value;
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
    return p -> palette[palette_addr];
}

void update_mirroring(pbus p){
    switch (p -> mapper -> get_mirror_type()) {
        case MIRROR_VERTICAL:
            p -> nametable0 = p -> nametable1 = 0;
            p -> nametable2 = p -> nametable3 = 0x400;
            break;
        case MIRROR_HORIZONTAL:
            p -> nametable0 = p -> nametable2 = 0;
            p -> nametable1 = p -> nametable3 = 0x400;
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
            perror("Unsupported Name Table Mirroring -> %s", p -> mapper -> get_mirror_type());
    }
}

void scanline_IRQ(pbus p){
    scanlineIRQ(p -> mapper);
}
