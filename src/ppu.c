//
// Created by Dario Bonfiglio on 10/9/25.
//

#include "headers/ppu.h"
#include <string.h>   // memset

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
    // Provide a safe default power-on palette (black/greys) until game programs $3F00-$3F1F
    static const uint8_t PAL_BOOT[32] = {
        0x0F,0x00,0x10,0x20,  0x0F,0x00,0x10,0x20,
        0x0F,0x00,0x10,0x20,  0x0F,0x00,0x10,0x20,
        0x0F,0x00,0x10,0x20,  0x0F,0x00,0x10,0x20,
        0x0F,0x00,0x10,0x20,  0x0F,0x00,0x10,0x20
    };
    memcpy(ppu->palette, PAL_BOOT, 32);

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
    // Provide a safe default power-on palette (black/greys) until game programs $3F00-$3F1F
    static const uint8_t PAL_BOOT[32] = {
        0x0F,0x00,0x10,0x20,  0x0F,0x00,0x10,0x20,
        0x0F,0x00,0x10,0x20,  0x0F,0x00,0x10,0x20,
        0x0F,0x00,0x10,0x20,  0x0F,0x00,0x10,0x20,
        0x0F,0x00,0x10,0x20,  0x0F,0x00,0x10,0x20
    };
    memcpy(ppu->palette, PAL_BOOT, 32);
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
    if (a < 0x2000) {
        // Pattern tables ($0000-$1FFF) via mapper (CHR-ROM/RAM)
        return ppu->mapper->chr_read(ppu->mapper, a);
    } else if (a < 0x3F00) {
        // Nametables region ($2000-$2FFF) and its mirror ($3000-$3EFF)
        if (a < 0x3000) {
            return ppu->ciram[nt_phys_index(ppu, a)];
        } else {
            // $3000-$3EFF mirrors $2000-$2EFF
            return ppu->ciram[nt_phys_index(ppu, (uint16_t)(a - 0x1000))];
        }
    } else {
        // Palette RAM indexes ($3F00-$3FFF) with internal mirroring
        return ppu->palette[pal_index_fix((uint8_t)a)];
    }
}

void ppu_write(ppu ppu, uint16_t addr, uint16_t value){
    uint16_t a = addr & 0x3FFF;
    uint8_t v = (uint8_t)(value & 0xFF);
    if (a < 0x2000) {
        // Pattern tables ($0000-$1FFF) via mapper (CHR-ROM/RAM)
        ppu->mapper->chr_write(ppu->mapper, a, v);
    } else if (a < 0x3F00) {
        // Nametables region ($2000-$2FFF) and mirror ($3000-$3EFF)
        if (a < 0x3000) {
            ppu->ciram[nt_phys_index(ppu, a)] = v;
        } else {
            ppu->ciram[nt_phys_index(ppu, (uint16_t)(a - 0x1000))] = v;
        }
    } else {
        // Palette RAM indexes ($3F00-$3FFF) with mirroring of 0x10/0x14/0x18/0x1C
        ppu->palette[pal_index_fix((uint8_t)a)] = v;
    }
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
            break;
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
// --- NTSC NES master palette (RGBA). Common approximation used by many emulators.
// Indexing uses values stored in PPU palette RAM ($3F00-$3F1F).
static const uint32_t NES_PAL_RGBA[64] = {
    0xFF757575,0xFF271B8F,0xFF0000AB,0xFF47009F,0xFF8F0077,0xFFA7004E,0xFFA70015,0xFF7F0B00,
    0xFF432F00,0xFF004700,0xFF005100,0xFF003F17,0xFF1B3F5F,0xFF000000,0xFF000000,0xFF000000,
    0xFFBCBCBC,0xFF0073EF,0xFF233BEF,0xFF8300F3,0xFFBF00BF,0xFFE7005B,0xFFDB2B00,0xFFCB4F0F,
    0xFF8B7300,0xFF009700,0xFF00AB00,0xFF00933B,0xFF00838B,0xFF000000,0xFF000000,0xFF000000,
    0xFFFFFFFF,0xFF3FBFFF,0xFF5F73FF,0xFFA74BFF,0xFFF04FBF,0xFFFF6F73,0xFFFF7B3F,0xFFEF9B23,
    0xFFDFBF13,0xFF4FDF4B,0xFF58F898,0xFF00EBDB,0xFF5F73FF,0xFF000000,0xFF000000,0xFF000000,
    0xFFFFFFFF,0xFFA7E7FF,0xFFBFB3FF,0xFFD7A3FF,0xFFFFA3E7,0xFFFFBFB3,0xFFFFCF9B,0xFFF7DF9B,
    0xFFFFF3B3,0xFFB3FFCF,0xFF9FFFF3,0xFFA3E7FF,0xFFBFB3FF,0xFF000000,0xFF000000,0xFF000000
};

// Convert NES color index (0..63) to RGBA.
static inline uint32_t nes_idx_to_rgba(uint8_t idx) {
    return NES_PAL_RGBA[idx & 0x3F];
}

void ppu_get_frame_rgba(ppu ppu, uint32_t *dst_rgba){
    const int NES_W = 256;
    const int NES_H = 240;

    // Safety
    if (!ppu || !dst_rgba) return;

    // Clear to a dark background
    uint32_t clear = 0xFF101010u;
    for (int y = 0; y < NES_H; ++y){
        for (int x = 0; x < NES_W; ++x){
            dst_rgba[(size_t)y * (size_t)NES_W + (size_t)x] = clear;
        }
    }
    if (!ppu->mapper || !ppu->mapper->chr_read) return;

    // Choose BG pattern table base from PPUCTRL bit4 (0=$0000, 1=$1000)
    uint16_t chr_base = (ppu->ppuctrl & 0x10) ? 0x1000 : 0x0000;

    // Universal background color at $3F00
    uint8_t universal = ppu->palette[0x00];

    // Render nametable 0 ($2000) only, 32x30 tiles
    for (int y = 0; y < NES_H; ++y){
        int tileY  = y >> 3;          // 0..29
        int fineY  = y & 7;           // 0..7
        for (int x = 0; x < NES_W; ++x){
            int tileX = x >> 3;       // 0..31
            int fineX = x & 7;        // 0..7

            // Name table byte: tile index
            uint16_t ntAddr = (uint16_t)(0x2000 + tileY * 32 + tileX);
            uint8_t  tile   = ppu->ciram[nt_phys_index(ppu, ntAddr)];

            // Attribute table: 1 byte covers a 4x4 tiles area; choose 2-bit palette
            uint16_t atAddr = (uint16_t)(0x23C0 + (tileY >> 2) * 8 + (tileX >> 2));
            uint8_t  atByte = ppu->ciram[nt_phys_index(ppu, atAddr)];
            int quadX = (tileX & 2) ? 1 : 0;
            int quadY = (tileY & 2) ? 1 : 0;
            int shift = (quadY << 2) | (quadX << 1);   // (0,0)->0, (1,0)->2, (0,1)->4, (1,1)->6
            uint8_t palIndex2b = (uint8_t)((atByte >> shift) & 0x03); // 0..3

            // Fetch the two bitplanes for this scanline of the tile
            uint16_t tileAddr = (uint16_t)(chr_base + tile * 16 + fineY);
            uint8_t p0 = ppu->mapper->chr_read(ppu->mapper, tileAddr);
            uint8_t p1 = ppu->mapper->chr_read(ppu->mapper, (uint16_t)(tileAddr + 8));

            int bit = 7 - fineX;
            int lo = (p0 >> bit) & 1;
            int hi = (p1 >> bit) & 1;
            int pix = (hi << 1) | lo;               // 0..3

            // Background palettes: four sets of 4 entries starting at $3F00.
            // For color 0, use universal background color ($3F00).
            uint8_t nes_color;
            if (pix == 0){
                nes_color = universal;
            } else {
                // $3F01..$3F03 (pal0), $3F05..$3F07 (pal1), ...
                uint8_t palBase = (uint8_t)(0x00 + palIndex2b * 4);
                nes_color = ppu->palette[palBase + pix];
            }

            dst_rgba[(size_t)y * (size_t)NES_W + (size_t)x] = nes_idx_to_rgba(nes_color);
        }
    }
}