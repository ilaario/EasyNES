//
// Created by Dario Bonfiglio on 10/9/25.
//

#ifndef EASYNES_PPU_H
#define EASYNES_PPU_H

#include <stdint.h>
#include <stdbool.h>
#ifdef LOG_DEBUG
#  undef LOG_DEBUG
#endif
#ifdef LOG_INFO
#  undef LOG_INFO
#endif
#ifdef LOG_WARNING
#  undef LOG_WARNING
#endif
#include <raylib.h>

#include "mapper.h"
#include "pbus.h"

typedef struct CPU* cpu;

typedef struct {
    int width;
    int height;
    Color *pixels;     // length = width * height
    Texture2D tex;     // opzionale: texture GPU per disegnare veloce
} PictureBuffer;

typedef PictureBuffer* p_buffer;

typedef struct {
    uint8_t *data;     // array dinamico di byte
    size_t size;       // quanti elementi sono usati (come .size())
    size_t capacity;   // capacità allocata (come .capacity())
} ByteVector;

typedef ByteVector* bv;

#define PB_INDEX(pb, x, y) ((y)*(pb)->width + (x))


#define SCANLINE_CYCLE_LENGTH   341
#define SCANLINE_END_CYCLE      340
#define VISIBLE_SCANLINE        240
#define SCANLINE_VISIBLE_DOTS   256
#define FRAME_END_SCANLINE      261

typedef enum ppu_state{
    PRE_RENDER      = 0,
    RENDER          = 1,
    POST_RENDER     = 2,
    VERTICAL_BLANK  = 3
} ppu_state;

typedef enum character_page{
    LOW = 0,
    HIGH = 1
} character_page;

struct Mapper;

struct PPU {
    void (*vblank_callback)(cpu);
    pbus bus;
    bv sprite_memory;
    bv scanline_sprites;

    ppu_state pipeline_state;

    int cycle;
    int scanline;
    bool even_frame;

    bool vblank;
    bool sprite_zero_hit;
    bool sprite_overflow;

    uint16_t data_address, temp_address;        // current/temporary VRAM address
    uint8_t fine_x_scroll;                      // fine X (3 bit)
    uint8_t first_write;                        // write toggle (0/1)
    uint8_t data_buffer;

    uint8_t sprite_data_address;

    bool long_sprite;
    bool generate_interrupt;

    bool grayscale_mode;
    bool show_sprites;
    bool show_background;
    bool hide_edge_sprites;
    bool hide_edge_backgound;

    character_page bg_page;
    character_page spr_page;

    uint16_t data_address_increment;

    p_buffer picture_buffer;
};

typedef struct PPU* ppu;

void create_ppu(ppu pp, pbus pb);
void step(ppu pp, cpu c);
void reset(ppu pp);

void setInterruptCallback(ppu pp, void(*cb)(cpu));

void doDMA(ppu pp, uint8_t* page_ptr);

// Callbacks mapped to CPU address space
// Addresses written to by the program
void control(ppu pp, uint8_t ctrl);
void setMask(ppu pp, uint8_t mask);
void setOAMAddress(ppu pp, uint8_t addr);
void setDataAddress(ppu pp, uint8_t addr);
void setScroll(ppu pp, uint8_t scroll);
void setData(ppu pp, uint8_t data);
// Read by the program
uint8_t getStatus(ppu pp);
uint8_t getData(ppu pp);
uint8_t getOAMData(ppu pp);
void setOAMData(ppu pp, uint8_t value);

void DEBUG_goto_scanline_dot(ppu ppu, int32_t scanline, int32_t dot);

static inline void PBSet(PictureBuffer *pb, int x, int y, Color c);
static inline Color PBGet(const PictureBuffer *pb, int x, int y);
bool PBInit(PictureBuffer *pb, int width, int height, Color fill);
void PBFree(PictureBuffer *pb);
void PBClear(PictureBuffer *pb, Color fill);
void PBFlushToGPU(PictureBuffer *pb);

#endif //EASYNES_PPU_H
