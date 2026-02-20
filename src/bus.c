//
// Created by Dario Bonfiglio on 10/9/25.
//

#include "headers/bus.h"
#include <string.h>

void skip_OAM_DMA_cycles(cpu c);

void bus_init(bus b, ppu p, apu a, cs c, void (*dma)(ppu, const uint8_t*)){
    b -> RAM = (uint8_t*)calloc(0x800, sizeof(uint8_t));
    b -> extRAM = NULL;
    b -> data_bus = 0;
    b -> address_bus = 0;
    b -> address_bus_is_write = false;
    b -> cpu_owner = NULL;
    b -> dma_callback = dma;
    b -> mapper = NULL;
    b -> ppu = p;
    b -> apu = a;
    b -> controller_set = c;
}

uint16_t normalise_mirror(uint16_t addr){
    if(addr >= PPU_CTRL && addr < APU_REGISTER_START) return addr & 0x2007;
    return addr;
}

static inline uint8_t open_bus(bus b) {
    return b -> data_bus;
}

static inline void latch_bus(bus b, uint8_t value) {
    b -> data_bus = value;
}

uint8_t bus_read(bus b, uint16_t addr){
    b -> address_bus = addr;
    b -> address_bus_is_write = false;
    if(addr < 0x2000) {
        uint8_t value = b -> RAM[addr & 0x7FF];
        latch_bus(b, value);
        return value;
    }
    else if(addr < 0x4020){
        addr = normalise_mirror(addr);

        switch (addr) {
            case PPU_STATUS: {
                uint8_t value = getStatus(b -> ppu);
                latch_bus(b, value);
                return value;
            }
            case PPU_DATA: {
                uint8_t value = getData(b -> ppu);
                latch_bus(b, value);
                return value;
            }
            case JOY1:
            case JOY2_AND_FRAME_CONTROL: {
                uint8_t value = (uint8_t)((open_bus(b) & 0xE0) |
                                          (controller_cpu_read(b -> controller_set, addr) & 0x01));
                latch_bus(b, value);
                return value;
            }
            case OAM_DATA: {
                uint8_t value = getOAMData(b -> ppu);
                latch_bus(b, value);
                return value;
            }
            case APU_CONTROL_AND_STATUS:
                // Hardware quirk: reading $4015 does not drive the CPU open bus latch.
                return read_status(b -> apu);
            case PPU_CTRL:
            case PPU_MASK:
            case OAM_ADDR:
            case PPU_SCROL:
            case PPU_ADDR: {
                uint8_t value = ppu_read_open_bus(b -> ppu);
                latch_bus(b, value);
                return value;
            }
            default:
                // Open bus on write-only/unmapped I/O.
                return open_bus(b);
        }
    }else if(addr < 0x6000){
        return open_bus(b);
    }else if(addr < 0x8000){
        if(hasExtendedRAM(b -> mapper) && b -> extRAM){
            uint8_t value = b -> extRAM[addr - 0x6000];
            latch_bus(b, value);
            return value;
        }

        return open_bus(b);
    } else {
        if (!b -> mapper || !b -> mapper -> cpu_read) return open_bus(b);
        uint8_t value = b -> mapper -> cpu_read(b -> mapper, addr);
        latch_bus(b, value);
        return value;
    }
}

void bus_write(bus b, uint16_t addr, uint8_t value){
    b -> address_bus = addr;
    b -> address_bus_is_write = true;
    // Every write drives the CPU data bus latch.
    latch_bus(b, value);
    if(addr < 0x2000) b -> RAM[addr & 0x7FF] = value;
    else if(addr < 0x4020){
        addr = normalise_mirror(addr);

        switch (addr) {
            case PPU_STATUS:
                // Writes to $2002 are ignored functionally, but still drive the PPU I/O latch.
                ppu_write_open_bus(b -> ppu, value);
                break;
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
            {
                if (!b -> dma_callback) break;
                if (b -> cpu_owner) skip_OAM_DMA_cycles(b -> cpu_owner);
                uint16_t base_addr = (uint16_t)value << 8;
                const uint8_t* page_ptr = getPagePtr(b, value);
                if (page_ptr) {
                    b -> dma_callback(b -> ppu, page_ptr);
                } else {
                    uint8_t page_copy[256];
                    for (int i = 0; i < 256; ++i) {
                        page_copy[i] = bus_read(b, (uint16_t)(base_addr + (uint16_t)i));
                    }
                    b -> dma_callback(b -> ppu, page_copy);
                }
                break;
            }
            case JOY1:
                controller_cpu_write(b -> controller_set, addr, value);
                break;
            case JOY2_AND_FRAME_CONTROL:
            case APU_CONTROL_AND_STATUS:
                write_register(b -> apu, addr, value);
                break;
            default:
                if(addr >= APU_REGISTER_START && addr <= APU_REGISTER_END) write_register(b -> apu, addr, value);
                break;
        }
    }else if(addr < 0x6000){

    }else if(addr < 0x8000){
        if(hasExtendedRAM(b -> mapper) && b -> extRAM){
            b -> extRAM[addr - 0x6000] = value;
        }
    } else {
        if (b -> mapper && b -> mapper -> cpu_write) b -> mapper -> cpu_write(b -> mapper, addr, value);
    }
}

bool setMapper(bus b, mapper mapper){
    b -> mapper = mapper;
    b -> extRAM = NULL;

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
    else if(addr < 0x4020) return NULL;
    else if(addr < 0x6000) return NULL;
    else if(addr < 0x8000) {
        if(hasExtendedRAM(b -> mapper) && b -> extRAM) return &b -> extRAM[addr - 0x6000];
    }
    return NULL;
}
