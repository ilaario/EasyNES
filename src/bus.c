//
// Created by Dario Bonfiglio on 10/9/25.
//

#include "headers/bus.h"

// —————————————————————————————————————————————————————
// Inizializzazione / Reset
// —————————————————————————————————————————————————————

void bus_init(bus bus, ppu ppu, mapper mapper, controller pad1, controller pad2){
    memset(bus -> ram, 0, 0x800);
    bus -> ppu              = ppu;
    bus -> mapper           = mapper;
    bus -> pad1             = pad1;
    bus -> pad2             = pad2;
    bus -> dma_active       = false;
    bus -> dma_src_page     = 0;
    bus -> dma_index        = 0;
    bus -> dma_even_cycle   = false;
}

void bus_reset(bus bus){
    memset(bus -> ram, 0, 0x800);
    bus -> dma_active       = false;
    bus -> dma_src_page     = 0;
    bus -> dma_index        = 0;
    bus -> dma_even_cycle   = false;
}


// —————————————————————————————————————————————————————
// Read helper: APU stub (temporaneo)
// —————————————————————————————————————————————————————

uint8_t apu_stub_read(uint16_t addr16){
    return 0x00;
}

void apu_stub_write(uint16_t addr16, uint8_t value){

}


// —————————————————————————————————————————————————————
// CPU reads
// —————————————————————————————————————————————————————

uint8_t bus_cpu_read8 (bus bus, uint16_t addr){
    if(addr <= 0x1FFF) {
        return bus -> ram[addr & 0x07FF];
    } else if(addr >= 0x2000 && addr <= 0x3FFF){
        uint16_t reg = 0x2000 + ((addr - 0x2000) & 0x7); // mirror ogni 8 registri ($2000-$2007)
        return ppu_reg_read(bus -> ppu, reg);
    } else if((addr >= 0x4000 && addr <= 0x4013) || addr == 0x4015){
        return apu_stub_read(addr);
    } else if(addr == 0x4014){
        return 0x0000;
    } else if(addr == 0x4016){
        return (controller_read_shift(bus -> pad1) & 0x01) | 0x40;
    } else if(addr == 0x4017){
        return (controller_read_shift(bus -> pad2) & 0x01) | 0x40;
    } else if(addr >= 0x4020 && addr <= 0x5FFF){
        return 0x0000;
    } else if(addr >= 0x6000){
        return bus -> mapper -> cpu_read(bus -> mapper, addr);
    }

    return 0x00;
}

void bus_cpu_write8(bus bus, uint16_t addr, uint8_t value){
    if(addr <= 0x1FFF) {
        bus -> ram[addr & 0x07FF] = value;
    } else if(addr >= 0x2000 && addr <= 0x3FFF){
        uint16_t reg = 0x2000 + ((addr - 0x2000) & 0x7); // mirror ogni 8 registri ($2000-$2007)
        ppu_reg_write(bus -> ppu, reg, value);
    } else if((addr >= 0x4000 && addr <= 0x4013) || addr == 0x4015){
        apu_stub_write(addr, value);
    } else if(addr == 0x4014){
        start_oam_dma(bus, value);
        bus -> dma_active = true;
    } else if(addr == 0x4016){
        controller_write_strobe(bus -> pad1, value & 0x01);
    } else if(addr == 0x4017){
        controller_write_strobe(bus -> pad2, value & 0x01);
    } else if(addr >= 0x4020 && addr <= 0x5FFF){

    } else if(addr >= 0x6000){
        bus -> mapper -> cpu_write(bus -> mapper, addr, value);
    }
}


// —————————————————————————————————————————————————————
// OAM DMA control
// —————————————————————————————————————————————————————

bool DEBUG_current_cpu_cycle_is_even(){
    return false;
}

void start_oam_dma(bus bus, uint8_t value){
    bus -> dma_active       = true;
    bus -> dma_src_page     = ((uint8_t)value) << 8;
    bus -> dma_index        = 0;
    bus -> dma_even_cycle   = DEBUG_current_cpu_cycle_is_even();
}

uint8_t dma_cpu_peek(bus bus, uint16_t addr){
    if(addr <= 0x1FFF){
        return bus -> ram[addr & 0x07FF];
    }
    if(addr >= 0x6000){
        return bus -> mapper -> cpu_read(bus -> mapper, addr);
    }
    return 0x00;
}

int bus_tick_dma_if_active(bus bus, /*maybe*/ uint64_t cpu_cycle){
    if(!bus -> dma_active) {
        return 0;
    }

    int cycle = 0;

    if(bus -> dma_even_cycle == false) {
        cycle += 1;
        bus -> dma_even_cycle = true;
    }

    while(bus -> dma_index < 256) {
        uint32_t src = bus -> dma_src_page | bus -> dma_index;
        uint8_t byte = dma_cpu_peek(bus, src);
        ppu_oam_dma_push_byte(bus -> ppu, byte);
        bus -> dma_index += 1;
        cycle += 2;
    }

    bus -> dma_active = false;
    return cycle;
}



