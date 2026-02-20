//
// Created by Dario Bonfiglio on 10/9/25.
//

#include "headers/ppu.h"
#include "headers/palette.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>   // memset

#define PPU_IOBUS_DECAY_CYCLES 3000000u

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

static inline void ppu_latch_io(ppu pp, uint8_t value) {
    pp -> io_bus = value;
    pp -> io_bus_decay = PPU_IOBUS_DECAY_CYCLES;
}

uint8_t ppu_read_open_bus(ppu pp) {
    if (!pp) return 0;
    uint8_t value = (pp -> io_bus_decay > 0) ? pp -> io_bus : 0;
    // CPU-visible reads from write-only PPU ports sample and refresh the I/O latch.
    ppu_latch_io(pp, value);
    return value;
}

void ppu_write_open_bus(ppu pp, uint8_t value) {
    if (!pp) return;
    ppu_latch_io(pp, value);
}

static void evaluate_scanline_sprites(ppu pp, int eval_scanline) {
    bv_resize(pp -> scanline_sprites, 0);

    if (eval_scanline < 0 || eval_scanline >= VISIBLE_SCANLINE) return;

    int range = pp -> long_sprite ? 16 : 8;
    size_t found = 0;
    for (size_t i = 0; i < 64; i++) {
        uint8_t spr_y = pp -> sprite_memory -> data[i * 4];
        int top = (int)spr_y + 1;
        if (eval_scanline >= top && eval_scanline < top + range) {
            if (found >= 8) {
                pp -> sprite_overflow = true;
                break;
            }
            bv_push(pp -> scanline_sprites, (uint8_t)i);
            ++found;
        }
    }
}

void create_ppu(ppu pp, pbus pb){
    if (!pp || !pb) exit(EXIT_FAILURE);
    pp -> bus = pb;
    pp -> sprite_memory = (bv)calloc(1, sizeof(ByteVector));
    pp -> scanline_sprites = (bv)calloc(1, sizeof(ByteVector));
    pp -> picture_buffer = (p_buffer)calloc(1, sizeof(PictureBuffer));
    if (!pp -> sprite_memory || !pp -> scanline_sprites || !pp -> picture_buffer) exit(EXIT_FAILURE);

    bv_init(pp -> sprite_memory, (64 * 4));
    bv_init(pp -> scanline_sprites, 8);
    if (!PBInit(pp -> picture_buffer, SCANLINE_VISIBLE_DOTS, VISIBLE_SCANLINE, BLACK)) exit(EXIT_FAILURE);
    reset(pp);
}

void step(ppu pp, cpu c){
    if (!pp) return;
    if (pp -> io_bus_decay > 0) {
        pp -> io_bus_decay -= 1;
        if (pp -> io_bus_decay == 0) pp -> io_bus = 0;
    }
    pp -> last_cpu = c;
    switch (pp -> pipeline_state) {
        case PRE_RENDER:
            if(pp -> cycle == 1){
                pp -> vblank = pp -> sprite_zero_hit = pp -> sprite_overflow = false;
            }else if(pp -> cycle == SCANLINE_VISIBLE_DOTS + 2 && (pp -> show_background || pp -> show_sprites)){
                pp->data_address &= ~0x41F;
                pp->data_address |= pp->temp_address & 0x41F;
            }else if(pp -> cycle > 280 && pp -> cycle <= 304 && (pp -> show_background || pp -> show_sprites)){
                pp->data_address &= ~0x7BE0;
                pp->data_address |= pp->temp_address & 0x7BE0;
            }

            if(pp -> cycle >= SCANLINE_END_CYCLE - (!pp -> even_frame && (pp -> show_background || pp -> show_sprites))){
                pp -> pipeline_state = RENDER;
                pp -> cycle = pp -> scanline = 0;
                evaluate_scanline_sprites(pp, 0);
            }

            if(pp -> cycle >= 260 && (pp -> show_background || pp -> show_sprites)) scanline_IRQ(pp -> bus);
            break;
        case RENDER:
            if(pp -> cycle > 0 && pp -> cycle <= SCANLINE_VISIBLE_DOTS){
                uint8_t bg_color = 0, spr_color = 0;
                bool bg_opaque = false, spr_opaque = false;
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
                        bg_color |= ((ppu_read(pp, addr + 8) >> (7 ^ x_fine)) & 1) << 1;

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

                if(pp -> show_sprites && (!pp -> hide_edge_sprites || x >= 8)){
                    for(int i = 0; i < pp -> scanline_sprites -> size; i++){
                        uint8_t sprite_index = pp -> scanline_sprites -> data[i];
                        int spr_x = pp -> sprite_memory -> data[sprite_index * 4 + 3];

                        int x_off = x - spr_x;
                        if (x_off < 0 || x_off >= 8) continue;

                        int spr_y = (int)pp -> sprite_memory -> data[sprite_index * 4 + 0] + 1;
                        uint8_t tile = pp -> sprite_memory -> data[sprite_index * 4 + 1];
                        uint8_t attribute = pp -> sprite_memory -> data[sprite_index * 4 + 2];

                        int length = (pp -> long_sprite) ? 16 : 8;
                        int y_offset = y - spr_y;
                        if (y_offset < 0 || y_offset >= length) continue;

                        int x_shift = x_off;

                        if ((attribute & 0x40) == 0) // If NOT flipping horizontally
                            x_shift ^= 7;
                        if ((attribute & 0x80) != 0) // IF flipping vertically
                            y_offset ^= (length - 1);

                        uint16_t addr = 0;

                        if(pp -> long_sprite){
                            int tile_row = y_offset >> 3;
                            int fine_y = y_offset & 0x07;
                            uint8_t tile_index = (uint8_t)((tile & 0xFE) + tile_row);
                            addr = ((uint16_t)(tile & 0x01) << 12) | ((uint16_t)tile_index << 4) | (uint16_t)fine_y;
                        } else {
                            addr = ((uint16_t)pp -> spr_page << 12) | ((uint16_t)tile << 4) | (uint16_t)(y_offset & 0x07);
                        }

                        uint8_t pixel = 0;
                        pixel |= (ppu_read(pp, addr) >> x_shift) & 1;
                        pixel |= ((ppu_read(pp, addr + 8) >> x_shift) & 1) << 1;

                        if(pixel == 0){
                            continue;
                        }

                        spr_color = pixel;
                        spr_opaque = true;
                        spr_color |= 0x10;
                        spr_color |= (attribute & 0x3) << 2;

                        sprite_fg  = !(attribute & 0x20);

                        bool is_sprite_zero = (i == 0);
                        if(!pp -> sprite_zero_hit && pp -> show_background &&
                           is_sprite_zero && spr_opaque && bg_opaque && x < 255) {
                            pp -> sprite_zero_hit = true;
                        }

                        break;
                    }
                }

                uint8_t palette_addr = bg_color;

                if ((!bg_opaque && spr_opaque) || (bg_opaque && spr_opaque && sprite_fg))
                    palette_addr = spr_color;
                else if (!bg_opaque && !spr_opaque)
                    palette_addr = 0;

                uint32_t col = colors[read_palette(pp->bus, palette_addr)];
                Color c = {
                        (unsigned char)((col >> 24) & 0xFF),  // R
                        (unsigned char)((col >> 16) & 0xFF),  // G
                        (unsigned char)((col >> 8)  & 0xFF),  // B
                        255                                   // A = fully opaque
                };
                PBSet(pp->picture_buffer, x, y, c);
            }else if(pp -> cycle == SCANLINE_VISIBLE_DOTS + 1 && pp -> show_background){
                if((pp -> data_address & 0x7000) != 0x7000) pp -> data_address += 0x1000;
                else{
                    pp -> data_address &= ~0x7000;
                    int y = (pp -> data_address & 0x03E0) >> 5;
                    if(y == 29) {
                        y = 0;
                        pp -> data_address ^= 0x0800;
                    } else if(y == 31) y = 0;
                    else y += 1;

                    pp -> data_address = ((pp -> data_address & ~0x03E0) | (y << 5));
                }
            }else if(pp -> cycle == SCANLINE_VISIBLE_DOTS + 2 && (pp -> show_background || pp -> show_sprites)){
                pp -> data_address &= ~0x41f;
                pp -> data_address |= pp -> temp_address & 0x41f;
            }

            if(pp -> cycle == 260 && (pp -> show_background || pp -> show_sprites)) scanline_IRQ(pp -> bus);
            if(pp -> cycle >= SCANLINE_END_CYCLE) {
                evaluate_scanline_sprites(pp, pp -> scanline + 1);

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
            }
            break;
        case VERTICAL_BLANK:
                if(pp -> cycle == 1 && pp -> scanline == VISIBLE_SCANLINE + 1){
                    pp -> vblank =  true;
                    if(pp -> generate_interrupt && pp -> vblank_callback) pp -> vblank_callback(c);
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

    pp -> cycle += 1;
}

void reset(ppu pp){
    pp -> long_sprite = pp -> generate_interrupt = pp -> grayscale_mode = pp -> vblank = pp -> sprite_overflow = pp -> sprite_zero_hit = false;
    pp -> show_background = pp -> show_sprites = pp -> even_frame = pp -> first_write = true;
    pp -> bg_page = pp -> spr_page = LOW;
    pp -> data_address = pp -> cycle = pp -> scanline = pp -> sprite_data_address = pp -> fine_x_scroll = pp -> temp_address = 0;
    // m_baseNameTable = 0x2000;
    pp -> data_address_increment = 1;
    pp -> io_bus = 0;
    pp -> io_bus_decay = 0;
    pp -> pipeline_state = PRE_RENDER;

    bv_reserve(pp -> scanline_sprites, 8);
    bv_resize(pp -> scanline_sprites, 0);
}

void setInterruptCallback(ppu pp, void(*cb)(cpu)){
    pp -> vblank_callback = cb;
}

uint8_t readOAM(ppu pp, uint16_t addr){
    return pp -> sprite_memory -> data[addr];
}

void writeOAM(ppu pp, uint16_t addr, uint8_t v){
    pp -> sprite_memory -> data[addr] = v;
}

void doDMA(ppu pp, const uint8_t* page_ptr){
    memcpy(pp -> sprite_memory -> data + pp -> sprite_data_address, page_ptr, 256 - pp -> sprite_data_address);
    if (pp -> sprite_data_address)
        memcpy(pp -> sprite_memory -> data, page_ptr + (256 - pp -> sprite_data_address), pp -> sprite_data_address);
}

void control(ppu pp, uint8_t ctrl){
    bool prev_generate_interrupt = pp -> generate_interrupt;
    pp -> generate_interrupt = (ctrl & 0x80) != 0;
    pp -> long_sprite        = ctrl & 0x20;
    pp -> bg_page            = (character_page)(!!(ctrl & 0x10));
    pp -> spr_page           = (character_page)(!!(ctrl & 0x8));

    if(ctrl & 0x4) pp -> data_address_increment = 0x20;
    else pp -> data_address_increment = 1;

    pp -> temp_address &= ~0xc00;
    pp -> temp_address |= (ctrl & 0x3) << 10;

    // Enabling NMI while already in VBlank should trigger an immediate NMI edge.
    if (!prev_generate_interrupt && pp -> generate_interrupt && pp -> vblank &&
        pp -> vblank_callback && pp -> last_cpu) {
        pp -> vblank_callback(pp -> last_cpu);
    }
    ppu_latch_io(pp, ctrl);
}

void setMask(ppu pp, uint8_t mask){
    pp -> grayscale_mode       = mask & 0x1;
    pp -> hide_edge_backgound  = !(mask & 0x2);
    pp -> hide_edge_sprites    = !(mask & 0x4);
    pp -> show_background      = mask & 0x8;
    pp -> show_sprites         = mask & 0x10;
    ppu_latch_io(pp, mask);
}

void setOAMAddress(ppu pp, uint8_t addr){
    pp -> sprite_data_address = addr;
    ppu_latch_io(pp, addr);
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
    ppu_latch_io(pp, addr);
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
    ppu_latch_io(pp, scroll);
}

void setData(ppu pp, uint8_t data){
    ppu_write(pp, pp -> data_address, data);
    pp -> data_address += pp -> data_address_increment;
    ppu_latch_io(pp, data);
}

uint8_t getStatus(ppu pp){
    uint8_t status = (uint8_t)(pp -> sprite_overflow << 5 | pp -> sprite_zero_hit << 6 | pp -> vblank << 7);
    status = (uint8_t)((status & 0xE0) | (pp -> io_bus & 0x1F));
    pp -> vblank      = false;
    pp -> first_write = true;
    ppu_latch_io(pp, status);
    return status;
}

uint8_t getData(ppu pp){
    uint16_t addr = pp -> data_address;
    uint8_t data = ppu_read(pp, addr);
    pp -> data_address += pp -> data_address_increment;

    if(addr < 0x3F00) {
        uint8_t tmp = data;
        data = pp -> data_buffer;
        pp -> data_buffer = tmp;
    } else {
        // Palette reads are immediate; keep the internal buffer coherent with mirrored nametable reads.
        pp -> data_buffer = ppu_read(pp, (uint16_t)(addr - 0x1000));
        data = (uint8_t)((data & 0x3F) | (pp -> io_bus & 0xC0));
    }

    ppu_latch_io(pp, data);
    return data;
}

uint8_t getOAMData(ppu pp){
    uint8_t value = 0;
    bool from_oam = true;
    bool rendering_enabled = pp -> show_background || pp -> show_sprites;
    bool visible_scanline = pp -> scanline >= 0 && pp -> scanline < VISIBLE_SCANLINE;

    if (rendering_enabled && visible_scanline) {
        if ((pp -> cycle >= 1 && pp -> cycle <= 64) ||
            (pp -> cycle >= 257 && pp -> cycle <= 320)) {
            value = 0xFF;
            from_oam = false;
        } else {
            value = readOAM(pp, pp -> sprite_data_address);
        }
    } else {
        value = readOAM(pp, pp -> sprite_data_address);
    }

    if (from_oam && (pp -> sprite_data_address & 0x03) == 0x02) value &= 0xE3;
    ppu_latch_io(pp, value);
    return value;
}

void setOAMData(ppu pp, uint8_t value){
    bool rendering_enabled = pp -> show_background || pp -> show_sprites;
    bool visible_scanline = pp -> scanline >= 0 && pp -> scanline < VISIBLE_SCANLINE;
    if (rendering_enabled && visible_scanline && pp -> cycle >= 1 && pp -> cycle <= 256) {
        pp -> sprite_data_address = (uint8_t)((pp -> sprite_data_address + 4) & 0xFC);
        ppu_latch_io(pp, value);
        return;
    }
    writeOAM(pp, pp -> sprite_data_address++, value);
    ppu_latch_io(pp, value);
}
