//
// Created by Dario Bonfiglio on 10/11/25.
//

#ifndef EASYNES_EMU_H
#define EASYNES_EMU_H


// emulator.h
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "raylib.h"

#include "cpu.h"
#include "apu/APU.h"
#include "ppu.h"
#include "pbus.h"
#include "cartridge.h"
#include "mapper.h"
#include "controller.h"
#include "bus.h"
#include "audio_player.h"

/* Dimensioni video NES (visibile) */
enum {
    NESVideoWidth  = SCANLINE_VISIBLE_DOTS,   /* definiscile come nel tuo progetto */
    NESVideoHeight = VISIBLE_SCANLINE
};

/* TimePoint/Duration: usiamo nanosecondi (uint64_t) */
typedef uint64_t TimePointNS;
typedef uint64_t DurationNS;

/* Mappatura tasti con raylib */
typedef struct {
    int p1_keys[8];   /* KeyboardKey di raylib (KEY_*) */
    int p2_keys[8];
    int p1_count;
    int p2_count;
} KeyMap;

/* Emulator “classe” -> struct C */
typedef struct {
    /* Sottosistemi */
    cpu         cpu;
    apu         apu;
    ppu         ppu;
    pbus        picture_bus;
    cartridge   cartridge;
    mapper      mapper;          /* equivalente a unique_ptr<Mapper> */
    cs          controller_set;
    bus         bus;

    /* Audio/video */
    audio_player     audio_player;
    p_buffer         emulator_screen;  /* Color* + Texture2D per il draw */

    /* Stato finestra/rendering (raylib gestisce la window globalmente) */
    float       screen_scale;
    int         video_width;
    int         video_height;

    /* Tempo */
    TimePointNS last_wakeup_ns;
    DurationNS  elapsed_ns;

    /* Input */
    KeyMap      keys;

    /* Flag utility */
    bool        audio_muted;
} Emulator;

/* Costruttore/distruttore */
void emulator_init(Emulator *emu);
void emulator_dispose(Emulator *emu);

/* Avvio ROM + main loop */
void emulator_run(Emulator *emu, const char *rom_path);

/* Settaggi video */
void emulator_set_video_width (Emulator *emu, int width);
void emulator_set_video_height(Emulator *emu, int height);
void emulator_set_video_scale (Emulator *emu, float scale);

/* Tasti */
void emulator_set_keys(Emulator *emu,
                       const int *p1_keys, int p1_count,
                       const int *p2_keys, int p2_count);

/* Audio */
void emulator_mute_audio(Emulator *emu, bool mute);

/* DMA (private in C++, public API qui o dichiarale static nel .c) */
void emulator_oamdma(Emulator* e, uint8_t page);
uint8_t emulator_dmcdma(cpu c, uint16_t addr);

#endif //EASYNES_EMU_H
