//
// Created by Dario Bonfiglio on 10/9/25.
//

#include "headers/ppu.h"
#include "headers/palette.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>   // memset

void bv_init(bv v, size_t reserve_capacity) {
    v -> data = calloc(reserve_capacity, sizeof(uint8_t));
    v -> size = 0;
    v -> capacity = reserve_capacity;
}

void bv_reserve(bv v, size_t new_cap) {
    if (new_cap > v->capacity) {
        v->data = realloc(v->data, new_cap * sizeof(uint8_t));
        v->capacity = new_cap;
    }
}

void bv_resize(bv v, size_t new_size) {
    if (new_size > v->capacity) {
        v->data = realloc(v->data, new_size * sizeof(uint8_t));
        v->capacity = new_size;
    }
    v->size = new_size;
}

void bv_push(bv v, uint8_t value) {
    if (v->size >= v->capacity) {
        v->capacity = v->capacity ? v->capacity * 2 : 1;
        v->data = realloc(v->data, v->capacity * sizeof(uint8_t));
    }
    v->data[v->size++] = value;
}

void bv_clear(bv v) {
    v->size = 0;   // come resize(0)
}

void bv_free(bv v) {
    free(v->data);
    v->data = NULL;
    v->size = v->capacity = 0;
}

static inline void PBSet(PictureBuffer *pb, int x, int y, Color c) {
    pb->pixels[PB_INDEX(pb, x, y)] = c;
}

static inline Color PBGet(const PictureBuffer *pb, int x, int y) {
    return pb->pixels[PB_INDEX(pb, x, y)];
}

bool PBInit(PictureBuffer *pb, int width, int height, Color fill) {
    pb->width  = width;
    pb->height = height;
    pb->pixels = (Color*)malloc(sizeof(Color) * (size_t)width * (size_t)height);
    if (!pb->pixels) return false;

    // riempi con un colore iniziale (es. MAGENTA)
    for (int i = 0; i < width*height; ++i) pb->pixels[i] = fill;

    // (opzionale) crea una texture da aggiornare ogni frame
    Image img = {
            .data = pb->pixels,
            .width = width,
            .height = height,
            .mipmaps = 1,
            .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8
    };
    pb->tex = LoadTextureFromImage(img);   // copia iniziale nella GPU
    return true;
}

void PBFree(PictureBuffer *pb) {
    if (pb->tex.id) UnloadTexture(pb->tex);
    free(pb->pixels);
    pb->pixels = NULL;
    pb->width = pb->height = 0;
}

void PBClear(PictureBuffer *pb, Color fill) {
    for (int i = 0; i < pb->width * pb->height; ++i) pb->pixels[i] = fill;
}

// chiama questo dopo aver modificato i pixel per “spingere” sulla GPU
void PBFlushToGPU(PictureBuffer *pb) {
    UpdateTexture(pb->tex, pb->pixels);
}

uint8_t ppu_read(ppu pp, uint16_t addr){
    return pbread(pp -> bus, addr);
}

void ppu_write(ppu pp, uint16_t addr, uint8_t v){
    pbwrite(pp -> bus, addr, v);
}

void create_ppu(ppu pp, pbus pb){
    pp -> bus = pb;
    bv_init(pp -> sprite_memory, (64 * 4));
    bv_init(pp -> scanline_sprites, 0);
    PBInit(pp -> picture_buffer, SCANLINE_VISIBLE_DOTS, VISIBLE_SCANLINE, MAGENTA);
}

void step(ppu pp){
    switch (pp -> pipeline_state) {
        case PRE_RENDER:
            if(pp -> cycle == 1){
                pp -> vblank = pp -> sprite_zero_hit = false;
            }else if(pp -> cycle == SCANLINE_VISIBLE_DOTS + 2 && pp -> show_background && pp -> show_sprites){
                pp->data_address &= ~0x41F;
                pp->data_address |= pp->temp_address & 0x41F;
            }else if(pp -> cycle > 280 && pp -> cycle <= 304 && pp -> show_background && pp -> show_sprites){
                pp->data_address &= ~0x7BE0;
                pp->data_address |= pp->temp_address & 0x7BE0;
            }

            if(pp -> cycle >= SCANLINE_END_CYCLE - (!pp -> even_frame && pp -> show_background && pp -> show_sprites)){
                pp -> pipeline_state = RENDER;
                pp -> cycle = pp -> scanline = 0;
            }

            if(pp -> cycle >= 260 && pp -> show_background && pp -> show_sprites) scanline_IRQ(pp -> bus);
            break;
        case RENDER:
            if(pp -> cycle > 0 && pp -> cycle <= SCANLINE_VISIBLE_DOTS){
                uint8_t bg_color = 0, spr_color = 0;
                bool bg_opaque = false, spr_opaque = true;
                bool sprite_fg = false;

                int x = pp -> cycle - 1;
                int y = pp -> scanline;

                if(pp -> show_background){
                    uint8_t x_fine = (pp -> fine_x_scroll + x) % 8;
                    if(!pp -> hide_edge_backgound || x >= 8){
                        uint16_t addr = 0x2000 | (pp -> data_address & 0x0FFF);
                        uint8_t tile = ppu_read(pp, addr);

                        addr = (tile * 16) + ((pp -> data_address >> 12) & 0x7);
                        addr |= pp -> bg_page << 12;

                        bg_color = (ppu_read(pp, addr) >> (7 ^ x_fine)) & 1;
                        bg_color = ((ppu_read(pp, addr + 8) >> (7 ^ x_fine)) & 1) << 1;

                        bg_opaque = bg_color;

                        addr = 0x23C0 | (pp -> data_address & 0x0C00) | ((pp -> data_address >> 4) & 0x38) | ((pp -> data_address >> 2) & 0x07);
                        uint8_t attribute = ppu_read(pp, addr);
                        int shift = ((pp -> data_address >> 4) & 4) | (pp -> data_address & 2);

                        bg_color |= ((attribute >> shift) & 0x3) << 2;
                    }

                    if(x_fine == 7){
                        if((pp -> data_address & 0x001F) == 31){
                            pp -> data_address &= ~0x001F;
                            pp -> data_address ^= 0x0400;
                        }else{
                            pp -> data_address += 1;
                        }
                    }
                }

                if(pp -> show_sprites && (pp -> hide_edge_sprites || x >= 8)){
                    for(int i = 0; i < pp -> scanline_sprites -> size; i++){
                        uint8_t spr_x = pp -> sprite_memory -> data[i * 4 + 2];

                        if (0 > x - spr_x || x - spr_x >= 8) continue;

                        uint8_t spr_y = pp -> sprite_memory -> data[i * 4 + 0] + 1,
                                tile = pp -> sprite_memory -> data[i * 4 + 1],
                                attribute = pp -> sprite_memory -> data[i * 4 + 2];

                        int length = (pp -> long_sprite) ? 16 : 8;

                        int x_shift = (x - spr_x) % 8, y_offset = (y - spr_y) % length;

                        if ((attribute & 0x40) == 0) // If NOT flipping horizontally
                            x_shift ^= 7;
                        if ((attribute & 0x80) != 0) // IF flipping vertically
                            y_offset ^= (length - 1);

                        uint16_t addr = 0;

                        if(pp -> long_sprite){
                            addr = tile * 16 + y_offset;
                            if(pp -> spr_page == HIGH) addr += 0x1000;
                        }else{
                            y_offset = (y_offset & 7) | ((y_offset & 8) << 1);
                            addr = (tile >> 1) * 32 + y_offset;
                            addr |= (tile & 1) << 12;
                        }

                        spr_color |= (ppu_read(pp, addr) >> (x_shift)) & 1;
                        spr_color |= ((ppu_read(pp, addr + 8) >> (x_shift)) & 1) << 1;

                        if(!(spr_opaque = spr_color)){
                            spr_color = 0;
                            continue;
                        }

                        spr_color |= 0x10;
                        spr_color |= (attribute & 0x3) << 2;

                        sprite_fg  = !(attribute & 0x20);

                        if(!pp -> sprite_zero_hit && pp -> show_background && i == 0 && spr_opaque && bg_opaque) pp -> sprite_zero_hit = true;

                        break;
                    }
                }

                uint8_t palette_addr = bg_color;

                if ((!bg_opaque && spr_opaque) || (bg_opaque && spr_opaque && sprite_fg))
                    palette_addr = spr_color;
                else if (!bg_opaque && !spr_opaque)
                    palette_addr = 0;

                PBSet(pp -> picture_buffer, x, y, colors[read_palette(pp -> bus, palette_addr)]);
            }else if(pp -> cycle == SCANLINE_VISIBLE_DOTS + 1 && pp -> show_background){
                if((pp -> data_address & 0x7000) != 0x7000) pp -> data_address += 0x1000;
                else{
                    pp -> data_address &= ~0x7000;
                    int y = (pp -> data_address & 0x03E0) >> 5;
                    if(y == 29) {
                        y = 0;
                        pp -> data_address ^= 0x8000;
                    } else if(y == 31) y = 0;
                    else y += 1;

                    pp -> data_address = ((pp -> data_address & ~0x03E0) | (y << 5));
                }
            }else if(pp -> cycle == SCANLINE_VISIBLE_DOTS + 2 && pp -> show_background && pp -> show_sprites){
                pp -> data_address &= ~0x41f;
                pp -> data_address |= pp -> temp_address & 0x41f;
            }

            if(pp -> cycle == 260 && pp -> show_background && pp -> show_sprites) scanline_IRQ(pp -> bus);
            if(pp -> cycle >= SCANLINE_END_CYCLE) {
                bv_resize(pp -> scanline_sprites, 0);

                int range = pp -> long_sprite ? 16 : 8;
                size_t j = 0;
                for(size_t i = pp -> sprite_data_address / 4; i < 64; i++){
                    uint16_t diff = (pp -> scanline - pp -> sprite_memory -> data[i * 4]);
                    if(0 <= diff && diff < range){
                        if(j >= 8){
                            pp -> sprite_overflow = true;
                            break;
                        }
                        bv_push(pp -> scanline_sprites, i);
                        ++j;
                    }
                }

                ++pp -> scanline;
                pp -> cycle = 0;
            }

            if(pp -> scanline >= VISIBLE_SCANLINE) pp -> pipeline_state = POST_RENDER;
            break;
        case POST_RENDER:
            if(pp -> cycle == SCANLINE_END_CYCLE){
                ++pp -> scanline;
                pp -> cycle = 0;
                pp -> pipeline_state = VERTICAL_BLANK;

                size_t size = (size_t)pp -> picture_buffer -> width * (size_t)pp -> picture_buffer -> height;
                for(size_t x = 0; x < size; ++x){
                    for(size_t y = 0; y < pp -> picture_buffer -> width, ++y){
                        // TODO -> implement screen
                    }
                }
            }
            break;
        case VERTICAL_BLANK:
                if(pp -> cycle == 1 && pp -> scanline == VISIBLE_SCANLINE + 1){
                    pp -> vblank =  true;
                    if(pp -> generate_interrupt) pp -> vblank_callback();
                }

                if(pp -> cycle >= SCANLINE_END_CYCLE){
                    ++pp -> scanline;
                    pp -> cycle = 0;
                }

                if(pp -> scanline >= FRAME_END_SCANLINE){
                    pp -> pipeline_state = PRE_RENDER;
                    pp -> scanline = 0;
                    pp -> even_frame = !pp -> even_frame;
                }
            break;
        default:
            perror("Logic error in PPU step");
            exit(EXIT_FAILURE);
    }
}

void reset(ppu pp){
    pp -> long_sprite = pp -> generate_interrupt = pp -> grayscale_mode = pp -> vblank = pp -> sprite_overflow = false;
    pp -> show_background = pp -> show_sprites = pp -> even_frame = pp -> first_write = true;
    pp -> bg_page = pp -> spr_page = LOW;
    pp -> data_address = pp -> cycle = pp -> scanline = pp -> sprite_data_address = pp -> fine_x_scroll = pp -> temp_address = 0;
    // m_baseNameTable = 0x2000;
    pp -> data_address_increment = 1;
    pp -> pipeline_state = PRE_RENDER;

    bv_reserve(pp -> scanline_sprites, 8);
    bv_resize(pp -> scanline_sprites, 0);
}

void setInterruptCallback(ppu pp, void(*cb)(void)){
    pp -> vblank_callback = cb;
}

uint8_t readOAM(ppu pp, uint16_t addr){
    return pp -> sprite_memory -> data[addr];
}

void writeOAM(ppu pp, uint16_t addr, uint8_t v){
    pp -> sprite_memory -> data[addr];
}

void doDMA(ppu pp, const uint8_t* page_ptr){
    memcpy(pp -> sprite_memory -> data + pp -> sprite_data_address, page_ptr, 256 - pp -> sprite_data_address);
    if (pp -> sprite_data_address)
        memcpy(pp -> sprite_memory -> data, page_ptr + (256 - pp -> sprite_data_address), pp -> sprite_data_address);
}

void control(ppu pp, uint8_t ctrl){
    pp -> generate_interrupt = ctrl & 0x80;
    pp -> long_sprite        = ctrl & 0x20;
    pp -> bg_page            = (character_page)(!!(ctrl & 0x10));
    pp -> spr_page           = (character_page)(!!(ctrl & 0x8));

    if(ctrl & 0x4) pp -> data_address_increment = 0x20;
    else pp -> data_address_increment = 1;

    pp -> temp_address &= ~0xc00;
    pp -> temp_address |= (ctrl & 0x3) << 10;
}

void setMask(ppu pp, uint8_t mask){
    pp -> grayscale_mode       = mask & 0x1;
    pp -> hide_edge_backgound  = !(mask & 0x2);
    pp -> hide_edge_sprites    = !(mask & 0x4);
    pp -> show_background      = mask & 0x8;
    pp -> show_sprites         = mask & 0x10;
}

void setOAMAddress(ppu pp, uint8_t addr){
    pp -> sprite_data_address = addr;
}

void setDataAddress(ppu pp, uint8_t addr){
    if (pp -> first_write){
        pp -> temp_address &= ~0xff00; // Unset the upper byte
        pp -> temp_address |= (addr & 0x3f) << 8;
        pp -> first_write  = false;
    } else {
        pp -> temp_address  &= ~0xff; // Unset the lower byte;
        pp -> temp_address  |= addr;
        pp -> data_address  = pp -> temp_address;
        pp -> first_write   = true;
    }
}
void setScroll(ppu pp, uint8_t scroll){
    if(pp -> first_write) {
        pp -> temp_address   &= ~0x1f;
        pp -> temp_address   |= (scroll >> 3) & 0x1f;
        pp -> fine_x_scroll  = scroll & 0x7;
        pp -> first_write    = false;
    } else {
        pp -> temp_address &= ~0x73e0;
        pp -> temp_address |= ((scroll & 0x7) << 12) | ((scroll & 0xf8) << 2);
        pp -> first_write   = true;
    }
}

void setData(ppu pp, uint8_t data){
    ppu_write(pp, pp -> data_address, data);
    pp -> data_address += pp -> data_address_increment;
}

uint8_t getStatus(ppu pp){
    uint8_t status    = pp -> sprite_overflow << 5 | pp -> sprite_zero_hit << 6 | pp -> vblank << 7;
    pp -> vblank      = false;
    pp -> first_write = true;
    return status;
}

uint8_t getData(ppu pp){
    uint8_t data = ppu_read(pp, pp -> data_address);
    pp -> data_address += pp -> data_address_increment;

    if(pp -> data_address < 0x3F00) {
        uint8_t tmp = data;
        data = pp -> data_buffer;
        pp -> data_buffer = tmp;
    }

    return data;
}

uint8_t getOAMData(ppu pp){
    return readOAM(pp, pp -> sprite_data_address);
}

void setOAMData(ppu pp, uint8_t value){
    writeOAM(pp, pp -> sprite_data_address++, value);
}