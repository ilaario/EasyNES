//
// Created by Dario Bonfiglio on 10/9/25.
//

#ifndef EASYNES_PPU_H
#define EASYNES_PPU_H

#include <stdint.h>
#include <stdbool.h>

#include "mapper.h"

struct Mapper;

struct PPU {
    uint16_t v, t;            // current/temporary VRAM address
    uint8_t  x;               // fine X (3 bit)
    uint8_t  w;               // write toggle (0/1)

    // PPU registers
    uint8_t ppuctrl;          // $2000
    uint8_t ppumask;          // $2001
    uint8_t ppustatus;        // $2002 -> use only bit 7/6/5

    // OAM
    uint8_t oam[256];
    uint8_t oamaddr;

    // PPU memory
    uint8_t ciram[2 * 1024];  // nametable ram
    uint8_t palette[32];      // $3F00 - $3F1F
    uint8_t read_buffer;      // buffer for PPUDATA reads

    // Timing
    int32_t scanline;         // 0 .. 261
    int32_t dot;              // 0 .. 340
    uint64_t ppu_cycle_accum;

    // Flags sprite
    bool sprite0_hit;
    bool sprite_overflow;

    mapper mapper;            // per accedere a CHR via mapper
    enum mirror_type nt_mirror;
    bool nmi_pending;
};

typedef struct PPU* ppu;

void     ppu_init(ppu ppu, mapper mapper, enum mirror_type nt_mirror);
void     ppu_reset(ppu ppu);

// accesso ai registri memory-mapped $2000–$2007
uint8_t  ppu_reg_read (ppu ppu, uint8_t reg_index);
void     ppu_reg_write(ppu ppu, uint8_t reg_index, uint8_t value);

// chiamata dal bus quando scrivi $4014
void     ppu_oam_dma_push_byte(ppu ppu, uint8_t value);

bool     ppu_nmi_pending(ppu ppu);
void     ppu_clear_nmi(ppu ppu);

void ppu_tick(ppu ppu, uint64_t cpu_cycles);
void step_one_ppu_cycle(ppu ppu);

void ppu_write(ppu ppu, uint16_t addr, uint16_t value);
uint8_t ppu_read(ppu ppu, uint16_t addr);

// Riempi un buffer RGBA8888 256x240 con il frame corrente
void ppu_get_frame_rgba(ppu ppu, uint32_t *dst_rgba);

void DEBUG_goto_scanline_dot(ppu ppu, int32_t scanline, int32_t dot);

#endif //EASYNES_PPU_H
