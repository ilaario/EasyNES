//
// Created by Dario Bonfiglio on 10/9/25.
//

#include "headers/bus.h"
#include <string.h>

void bus_init(bus b, ppu p, /*apu a,*/ cs c, void (*dma)(uint8_t)){
    b -> RAM = (uint8_t*)calloc(0x800, sizeof(uint8_t));
    b -> dma_callback = dma;
    b -> mapper = NULL;
    b -> ppu = p;
    // b -> apu = a;
    b -> controller_set = c;
}

uint16_t normalise_mirror(uint16_t addr){
    if(addr >= PPU_CTRL && addr < APU_REGISTER_START) return addr & 0x2007;
    return addr;
}

uint8_t bus_read(bus b, uint16_t addr){
    if(addr < 0x2000) return b -> RAM[addr & 0x7FF];
    else if(addr < 0x4020){
        addr = normalise_mirror(addr);

        switch (addr) {
            case PPU_STATUS:
                return getStatus(b -> ppu);
            case PPU_DATA:
                return getData(b -> ppu);
            case JOY1:
                return read_pad_1_as_bitmask(b -> controller_set);
            case JOY2_AND_FRAME_CONTROL:
                return read_pad_2_as_bitmask(b -> controller_set);
            case OAM_DATA:
                return getOAMData(b -> ppu);
            case APU_CONTROL_AND_STATUS:
                // return apu_readStatus(b -> apu);
                return 0x00;
            default:
                perror("Read access attempt at 0x%04X", addr);
                return 0;
        }
    }else if(addr < 0x6000){
        return 0x00;
    }else if(addr < 0x8000){
        if(hasExtendedRAM(b -> mapper)){
            return b -> extRAM[addr - 0x6000];
        }

        return 0x00;
    } else {
        return b -> mapper -> cpu_read(b -> mapper, addr);
    }
}

void bus_write(bus b, uint16_t addr, uint8_t value){
    if(addr < 0x2000) b -> RAM[addr & 0x7FF] = value;
    else if(addr < 0x4020){
        addr = normalise_mirror(addr);

        switch (addr) {
            case PPU_CTRL:
                control(b -> ppu, value);
                break;
            case PPU_MASK:
                setMask(b -> ppu, value);
                break;
            case OAM_ADDR:
                setOAMAddress(b -> ppu, value);
                break;
            case OAM_DATA:
                setOAMData(b -> ppu, value);
                break;
            case PPU_ADDR:
                setDataAddress(b -> ppu, value);
                break;
            case PPU_SCROL:
                setScroll(b -> ppu, value);
                break;
            case PPU_DATA:
                setData(b -> ppu, value);
                break;
            case OAM_DMA:
                b -> dma_callback(value);
            case JOY1:
                // m_controller1.strobe(value);
                // m_controller2.strobe(value);
                // break;
            case JOY2_AND_FRAME_CONTROL:
            case APU_CONTROL_AND_STATUS:
                // apu_writeReg(b -> apu, addr, value);
                break;
            default:
                if(addr >= APU_REGISTER_START && addr <= APU_REGISTER_END) break; // apu_writeReg(b -> apu, addr, value);
                else perror("Write access attempt at 0x%04X", addr);
                break;
        }
    }else if(addr < 0x6000){

    }else if(addr < 0x8000){
        if(hasExtendedRAM(b -> mapper)){
            b -> extRAM[addr - 0x6000] = value;
        }
    } else {
        b -> mapper -> cpu_write(b -> mapper, addr, value);
    }
}

bool setMapper(bus b, mapper mapper){
    b -> mapper = mapper;

    if(!mapper){
        perror("Mapper is null");
        return false;
    }

    if(hasExtendedRAM(b -> mapper)){
        b -> extRAM = (uint8_t*)calloc(0x2000, sizeof(uint8_t));
    }

    return true;
}

const uint8_t* getPagePtr(bus b, uint8_t page){
    uint16_t addr = page << 8;
    if(addr < 0x2000) return &b -> RAM[addr & 0x7FF];
    else if(addr < 0x4020) perror("Register address memory pointer access attempt");
    else if(addr < 0x6000) perror("Expansion ROM access attempted, which is unsupported");
    else if(addr < 0x8000) {
        if(hasExtendedRAM(b -> mapper)) return &b -> extRAM[addr & 0x6000];
    } else perror("Unexpected DMA request");
    return NULL;
}
