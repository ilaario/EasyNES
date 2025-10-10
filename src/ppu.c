//
// Created by Dario Bonfiglio on 10/9/25.
//

#include "headers/ppu.h"

const int32_t PPU_CYCLES_PER_SCANLINE = 341;
const int32_t NTSC_SCANLINES_PER_FRAME = 262;
const int32_t VBLANK_START_SCANLINE = 241;
const int32_t PRERENDER_SCANLINE = 261;

void ppu_init(ppu ppu, mapper mapper, enum mirror_type nt_mirror){
    ppu -> v = 0;
    ppu -> t = 0;
    ppu -> x = 0;
    ppu -> w = 0;

    // PPU registers
    ppu -> ppuctrl = 0;
    ppu -> ppumask = 0;
    ppu -> ppustatus = 0;

    // OAM

    memset(ppu -> oam, 0, 256);
    ppu -> oamaddr = 0;

    // PPU memory
    memset(ppu -> ciram, 0, 2 * 1024); // nametable ram
    memset(ppu -> palette, 0, 32);     // $3F00 - $3F1F
    ppu -> read_buffer = 0;;     // buffer for PPUDATA reads

    // Timing
    ppu ->  scanline = 0;
    ppu ->  dot = 0;
    ppu ->  ppu_cycle_accum = 0;

    ppu -> mapper = mapper;
    ppu -> nt_mirror = nt_mirror;
    ppu -> nmi_pending = false;
}

void ppu_reset(ppu ppu){
    ppu -> v = 0;
    ppu -> t = 0;
    ppu -> x = 0;
    ppu -> w = 0;

    // PPU registers
    ppu -> ppuctrl = 0;
    ppu -> ppumask = 0;
    ppu -> ppustatus = 0;

    // OAM

    memset(ppu -> oam, 0, 256);
    ppu -> oamaddr = 0;

    // PPU memory
    memset(ppu -> ciram, 0, 2 * 1024);
    memset(ppu -> palette, 0, 32);
    ppu -> read_buffer = 0;;
    ppu -> nmi_pending = false;

    // Timing
    ppu ->  scanline = 0;
    ppu ->  dot = 0;
    ppu ->  ppu_cycle_accum = 0;
}

uint16_t nt_phys_index(ppu ppu, uint16_t addr){
    uint16_t idx  = (addr - 0x2000) & 0x0FFF;
    uint16_t page = idx / 0x400;
    uint16_t offs = idx % 0x400;
    switch (ppu -> nt_mirror) {
        case MIRROR_VERTICAL:
            return (page % 2) * 0x400 + offs;  // 0,1,0,1
        case MIRROR_HORIZONTAL:
            return (page / 2) * 0x400 + offs;  // 0,0,1,1
        case MIRROR_FOUR_SCREEN:
            return page * 0x400 + offs;        // 4KiB
    }
}

uint8_t pal_index_fix(uint8_t addr){
    uint8_t i = addr & 0x1F;
    if(i == 0x10) i = 0x00;
    if(i == 0x14) i = 0x04;
    if(i == 0x18) i = 0x08;
    if(i == 0x1C) i = 0x0C;
    return i;
}

uint8_t ppu_read(ppu ppu, uint16_t addr){
    uint16_t a = addr & 0x3FFF;
    if(a <= 0x1FFF) return 0x00; // mapper_chr_read(ppu -> mapper, a);
    if(a <= 0x2FFF) return ppu -> ciram[nt_phys_index(ppu, a)];
    if(a <= 0x3FFF) return ppu -> ciram[nt_phys_index(ppu, a - 0x1000)];
    return ppu -> palette[pal_index_fix(a)];
}

void ppu_write(ppu ppu, uint16_t addr, uint16_t value){
    uint16_t a = addr & 0x3FFF;
    if(a <= 0x1FFF) /* no-op */ // mapper_chr_write(ppu -> mapper, a, value);
    if(a <= 0x2FFF) ppu -> ciram[nt_phys_index(ppu, a)] = value;
    if(a <= 0x3FFF) ppu -> ciram[nt_phys_index(ppu, a - 0x1000)] = value;
    ppu -> palette[pal_index_fix(a)] = value;
}

void write_2000(ppu ppu, uint8_t value){
    ppu -> ppuctrl = value;
    // bit 0-1 -> nametable base
    ppu -> t = (ppu -> t & ~0x0C00) | ((value & 0x03) << 10);
    // bit2: increment mode (0:+1, 1:+32)
    // bit3: sprite pattern table (8x8) base (0:$0000,1:$1000)
    // bit4: bg pattern table base (0:$0000,1:$1000)
    // bit5: sprite size (0:8x8,1:8x16)
    // bit7: NMI enable (usato nello Stadio 2)
}

void write_2001(ppu ppu, uint8_t value){
    ppu -> ppumask = value;
}

uint8_t read_2002(ppu ppu){
    uint8_t res = ppu -> ppustatus;
    // side-effect: clear VBlank (bit7) and reset toggle W
    ppu -> ppustatus &= ~0x80;
    ppu -> w = 0;
    return res;
}

void write_2003(ppu ppu, uint8_t value){
    ppu -> oamaddr = value;
}

void write_2004(ppu ppu, uint8_t value){
    ppu -> oam[ppu -> oamaddr] = value;
    ppu -> oamaddr = (ppu -> oamaddr + 1) & 0xFF;
}

uint8_t read_2004(ppu ppu){
    return ppu -> oam[ppu -> oamaddr];
}

void write_2005(ppu ppu, uint8_t val){
    if(ppu -> w == 0){
        // first write fine X and coarse X
        ppu -> x = val & 0x07;
        ppu -> t = (ppu -> t & ~0x001F) | (val >> 3); // coarse X -> 5b
        ppu -> w = 1;
    } else {
        // second write fine Y and coarse Y
        uint8_t fineY = val & 0x07;
        uint8_t coarseY = val >> 3;
        ppu -> t = (ppu -> t & ~0x73E0) | ((fineY & 0x7) << 12) | ((coarseY & 0x1F) << 5);
        ppu -> w = 0;
    }
}

void write_2006(ppu ppu, uint8_t val){
    if(ppu -> w == 0){
        // high byte (6 bit valid)
        ppu -> t = (ppu -> t & 0x00FF) | ((val & 0x3F) << 8);
        ppu -> w = 1;
    } else {
        // low byte, then v = t
        ppu -> t = (ppu -> t & 0x7F00) | val;
        ppu -> v = ppu -> t;
        ppu -> w = 0;
    }
}

void write_2007(ppu ppu, uint8_t val){
    ppu_write(ppu, ppu -> v, val);
    uint8_t inc = (ppu -> ppuctrl & 0x04) ? 32 : 1;
    ppu -> v = (ppu -> v + inc) & 0x3FFF;
}

uint8_t read_2007(ppu ppu){
    uint8_t inc = (ppu -> ppuctrl & 0x04) ? 32 : 1;
    uint8_t out;
    if((ppu -> v & 0x3FFF) < 0x3F00){
        out = ppu -> read_buffer;
        ppu -> read_buffer = ppu_read(ppu, ppu -> v);
    } else {
        // palette is not buffered
        out = ppu_read(ppu, ppu -> v);
        // still load buffer from (v - 0x1000 -> hw behaviour)
        ppu -> read_buffer = ppu_read(ppu, (ppu -> v - 0x1000) & 0x3FFF);
    }
    ppu -> v = (ppu -> v + inc) & 0x3FFF;
    return out;
}

// accesso ai registri memory-mapped $2000–$2007
uint8_t ppu_reg_read (ppu ppu, uint8_t reg_index){
    uint8_t reg = reg_index & 7;
    switch (reg) {
        case 2:
            return read_2002(ppu);
        case 4:
            return read_2004(ppu);
        case 7:
            return read_2007(ppu);
        default:
            return 0x00;
    }
}

void ppu_reg_write(ppu ppu, uint8_t reg_index, uint8_t value){
    uint8_t reg = reg_index & 7;
    switch (reg) {
        case 0:
            write_2000(ppu, value);
            break;
        case 1:
            write_2001(ppu, value);
            break;
        case 3:
            write_2003(ppu, value);
            break;
        case 4:
            write_2004(ppu, value);
            break;
        case 5:
            write_2005(ppu, value);
            break;
        case 6:
            write_2006(ppu, value);
            break;
        case 7:
            write_2007(ppu, value);
            break;
        default:
            /* no-op */
    }
}

// ==============================
// SCROLL INCREMENTS (Loopy logic)
// ==============================

// v e t: registri 15-bit (yyy NN YYYYY XXXXX)
// yyy = fine Y (bits 14–12)
// NNy = nametable bits (11,10)
// YYYYY = coarse Y (bits 9–5)
// XXXXX = coarse X (bits 4–0)

bool rendering_enabled(ppu ppu){
    return (((ppu -> ppumask & 0x08) != 0) || ((ppu -> ppumask & 0x10) != 0));
}

// Horizontal increment (coarse X)
// Executed every 8 cycles while tile fetch
void increment_coarse_x(ppu ppu){
    if((ppu -> v & 0x001F) == 31){
        ppu -> v = ppu -> v & ~0x001F;  // Coarse X = 0
        ppu -> v = ppu -> v ^ 0x0400;   // Toggle Nametable X (bit 10)
    } else {
        ppu -> v = ppu -> v + 1;
    }
}

// Vertical increment (fine Y / coarse Y)
// Executed at dot 256 of every visible scanline
void increment_y(ppu ppu){
    if((ppu -> v & 0x7000) != 0x7000){ // fineY < 7
        ppu -> v = ppu -> v + 0x1000;  // fineY++
    } else {
        ppu -> v = ppu -> v & ~0x7000; // fineY = 0
        uint16_t coarseY = (ppu -> v & 0x03E0) >> 5;

        if(coarseY == 29){
            coarseY = 0;
            ppu -> v = ppu -> v ^ 0x0800;
        } else if(coarseY == 31){
            coarseY = 0;               // no toggle (overflow area)
        } else {
            coarseY += 1;
        }

        ppu -> v = (ppu -> v & ~0x03E0) | (coarseY << 5);
    }
}

// Horizontal copy t -> v
// Executed at dot 257 if rendering is active
void copy_horizontal_t_to_v(ppu ppu){
    ppu -> v = (ppu -> v & ~0x041F) | (ppu -> t & 0x041F);
}

// Vertical copy t -> v
// Executed during pre-render line (scanline 261) on dot 280 - 304
void copy_vertical_t_to_v(ppu ppu){
    ppu -> v = (ppu -> v & ~0x7BE0) | (ppu -> t & 0x7BE0);
}

// Handle all increments/copy for the current cycle
// Called in step_one_ppu_cycle() after VBlank/NMI signal
void handle_scroll_increments(ppu ppu){
    if(!rendering_enabled(ppu)) return;
    if((ppu -> dot >= 1 && ppu -> dot <= 255) || (ppu -> dot >= 321 && ppu -> dot <= 336)){
        if((ppu -> dot % 8) == 0) increment_coarse_x(ppu);
    }
    if(ppu -> dot == 256) increment_y(ppu);
    if(ppu -> dot == 257) copy_horizontal_t_to_v(ppu);
    if((ppu -> scanline == 261) && (ppu -> dot >= 280 && ppu -> dot <= 304)) copy_vertical_t_to_v(ppu);
}

void step_one_ppu_cycle(ppu ppu){
    ppu -> dot += 1;
    if(ppu -> dot == PPU_CYCLES_PER_SCANLINE){
        ppu -> dot = 0;
        ppu -> scanline += 1;
        if(ppu -> scanline == NTSC_SCANLINES_PER_FRAME){
            ppu -> scanline = 0;
        }
    }

    if(ppu -> scanline == VBLANK_START_SCANLINE && ppu -> dot == 1){
        // get in VBlank
        ppu -> ppustatus |= 0x80;          // Set VBlank
        if((ppu -> ppuctrl & 0x80) != 0){  // Enable NMI
            ppu -> nmi_pending = true;
        }
    }

    if(ppu -> scanline == PRERENDER_SCANLINE && ppu -> dot == 1){
        // prerender line -> reset for new frame
        ppu -> ppustatus &= ~0x80;
        ppu -> sprite0_hit = false;
        ppu -> sprite_overflow = false;
        // no w reset
    }

    handle_scroll_increments(ppu);
}

void ppu_tick(ppu ppu, uint64_t cpu_cycles){
    ppu -> ppu_cycle_accum += cpu_cycles * 3;
    while (ppu -> ppu_cycle_accum > 0){
        step_one_ppu_cycle(ppu);
        ppu -> ppu_cycle_accum -= 1;
    }
}


// chiamata dal bus quando scrivi $4014
void ppu_oam_dma_push_byte(ppu ppu, uint8_t value){
    ppu -> oam[ppu -> oamaddr] = value;
    ppu -> oamaddr = (ppu -> oamaddr + 1) & 0xFF;
}

bool ppu_nmi_pending(ppu ppu){
    return ppu -> nmi_pending;
}

void ppu_clear_nmi(ppu ppu){
    ppu -> nmi_pending = false;
}

void DEBUG_goto_scanline_dot(ppu ppu, int32_t scanline, int32_t dot){
    const int32_t CYC_LINE = 341;
    int32_t target = scanline * CYC_LINE + dot;
    int32_t current = ppu->scanline * CYC_LINE + ppu->dot;
    int32_t remain = target - current;
    if (remain < 0) remain = 0;  // (o fai wrap a nuovo frame, a tua scelta)

    for (int i = 0; i < remain; i++){
        step_one_ppu_cycle(ppu);
    }
}