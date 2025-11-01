//
// Created by Dario Bonfiglio on 10/11/25.
//

#include "headers/emu.h"

// emulator.c
#include "headers/apu/constants.h"


#ifdef LOG_DEBUG
#  undef LOG_DEBUG
#endif
#ifdef printf
#  undef printf
#endif
#ifdef LOG_WARNING
#  undef LOG_WARNING
#endif

#include <raylib.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

typedef uint64_t TimePointNS;
typedef uint64_t DurationNS;

/* Timer monotono in ns */
static inline TimePointNS now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (TimePointNS)ts.tv_sec * 1000000000ull + (TimePointNS)ts.tv_nsec;
}


void pb_flush_to_gpu(p_buffer pb) {
    // Copia tutto il framebuffer (RGBA8) nella texture sulla GPU
    UpdateTexture(pb->tex, pb->pixels);
}

/* Costruttore-equivalente */
void emulator_init(Emulator *e){
    e -> cpu = (cpu)malloc(sizeof(struct CPU));
    e -> apu = (apu)malloc(sizeof(struct APU));
    e -> ppu = (ppu)malloc(sizeof(struct PPU));
    e -> picture_bus = (pbus)malloc(sizeof(struct picture_bus));
    e -> cartridge = (cartridge)malloc(sizeof(struct Cartridge));
    e -> controller_set = (cs)malloc(sizeof(struct controller_set));
    e -> bus = (bus)malloc(sizeof(struct CPUBus));
    e -> audio_player = (audio_player)malloc(sizeof(struct AudioPlayer));

    pbus_init(e -> picture_bus);
    create_ppu(e -> ppu, e -> picture_bus);
    controllerset_init(e -> controller_set);
    init_audio(e -> audio_player, 1.0 / APU_CLOCK_PERIOD_S);
    apu_init(e -> apu, e -> audio_player, create_IRQ_handler(e -> cpu), emulator_dmcdma);
    bus_init(e -> bus, e -> ppu, e -> apu, e -> controller_set, doDMA);

    /* Audio/video */
    audio_player     audio_player;
    p_buffer         emulator_screen;  /* Color* + Texture2D per il draw */


    setInterruptCallback(e->ppu,  nmi_interrupt);

    e->screen_scale  = 3.0f;
    e->video_width   = NESVideoWidth;
    e->video_height  = NESVideoHeight;

    // finestra + framebuffer (raylib)
    InitWindow((int)(e->video_width * e->screen_scale),
               (int)(e->video_height * e->screen_scale),
               "SimpleNES (raylib)");
    SetTargetFPS(60);

    // crea il buffer video/texture (tipo il tuo p_buffer)
    PBInit(e->emulator_screen, e->video_width, e->video_height, MAGENTA);

    e->last_wakeup_ns = now_ns();
    e->elapsed_ns     = 0;

    // audio
    init_audio(e->audio_player, (1.0 / APU_CLOCK_PERIOD_S);
}

/* Main loop */
void emulator_run(Emulator *e, const char *rom_path)
{
    e->cartridge = read_allocate_cartridge(rom_path);
    if (!e->cartridge) return;

    // Mapper (unique_ptr in C++ ⇒ puntatore in C)
    create_mapper(e -> mapper, e->cartridge,
                              create_IRQ_handler(e->cpu));
    if (!e->mapper) {
        perror("Creating Mapper failed. Probably unsupported.");
        return;
    }

    if (!setMapper(e->bus, e->mapper) || !set_mapper(e->picture_bus, e->mapper)) {
        return;
    }

    cpu_reset(e->cpu);
    reset(e->ppu);

    // schermo virtuale (se hai un wrapper tuo; altrimenti usa direttamente pb_* + raylib)
    // virtual_screen_create(...);  // opzionale

    e->last_wakeup_ns = now_ns();
    e->elapsed_ns     = 0;

    bool focus = true;
    bool pause = false;

    while (!WindowShouldClose()) {
        // ESC per uscire
        if (IsKeyPressed(KEY_ESCAPE)) break;

        // focus window
        bool windowFocused = IsWindowFocused();
        if (windowFocused && !focus) {
            focus = true;
            TimePointNS now = now_ns();
            printf("Gained focus. Removing %llu ns from timers",
                      (unsigned long long)(now - e->last_wakeup_ns));
            e->last_wakeup_ns = now;
        } else if (!windowFocused && focus) {
            focus = false;
            printf("Losing focus; paused.");
        }

        // toggle pausa con F2
        if (IsKeyPressed(KEY_F2)) {
            pause = !pause;
            if (!pause) {
                TimePointNS now = now_ns();
                printf("Unpaused. Removing %llu ns from timers",
                          (unsigned long long)(now - e->last_wakeup_ns));
                e->last_wakeup_ns = now;
            } else {
                printf("Paused.");
            }
        }

        // single-step ~1 frame con F3 (solo se in pausa)
        if (pause && IsKeyReleased(KEY_F3)) {
            for (int i = 0; i < 29781; ++i) {
                // PPU
                step(e->ppu, e->cpu);
                step(e->ppu, e->cpu);
                step(e->ppu, e->cpu);
                // CPU
                cpu_step(e->cpu);
                // APU
                apu_step(e->apu);
            }
        }

        // set log level con F4/F5 (opzionale)
        if (focus && IsKeyReleased(KEY_F4)) log_set_level(printf);
        if (focus && IsKeyReleased(KEY_F5)) log_set_level(LOG_VERBOSE);

        if (focus && !pause) {
            TimePointNS now = now_ns();
            e->elapsed_ns += (now - e->last_wakeup_ns);
            e->last_wakeup_ns = now;

            while (e->elapsed_ns > CPU_CLOCK_PERIOD_NS) {
                // PPU 3x per CPU 1x
                step(e->ppu, e->cpu);
                step(e->ppu, e->cpu);
                step(e->ppu, e->cpu);
                // CPU
                cpu_step(e->cpu);
                // APU
                apu_step(e->apu);

                e->elapsed_ns -= CPU_CLOCK_PERIOD_NS;
            }

            // flush video → GPU e disegna
            pb_flush_to_gpu(e->emulator_screen);

            BeginDrawing();
            ClearBackground(BLACK);
            DrawTexturePro(
                    e->emulator_screen->tex,
                    (Rectangle){0, 0, (float)e->video_width, (float)e->video_height},
                    (Rectangle){0, 0, e->video_width * e->screen_scale, e->video_height * e->screen_scale},
                    (Vector2){0, 0}, 0.0f, WHITE
            );
            EndDrawing();
        } else {
            // “sleep” quando non attivo
            WaitTime(1.0/60.0);
        }
    }
}

/* DMA OAM (PPU) */
void emulator_oamdma(Emulator* e, uint8_t page)
{
    skip_OAM_DMA_cycles(e -> cpu);
    uint8_t *page_ptr = getPagePtr(e -> bus, page);
    if (page_ptr) {
        doDMA(e->ppu, page_ptr);
    } else {
        perror("Can't get pageptr for DMA");
    }
}

/* DMA DMC (APU) */
uint8_t emulator_dmcdma(cpu c, uint16_t addr)
{
    skip_DMC_DMA_cycles(c);
    return bus_read(c -> bus, addr);
}

/* Video: set scale/size (solo cambio scale come in C++) */
void emulator_set_video_height(Emulator *e, int height)
{
    e->screen_scale = (float)height / (float)NESVideoHeight;
    printf("Scale: %.2f set. Screen: %dx%d",
              e->screen_scale,
              (int)(NESVideoWidth  * e->screen_scale),
              (int)(NESVideoHeight * e->screen_scale));
}

void emulator_set_video_width(Emulator *e, int width)
{
    e->screen_scale = (float)width / (float)NESVideoWidth;
    printf("Scale: %.2f set. Screen: %dx%d",
              e->screen_scale,
              (int)(NESVideoWidth  * e->screen_scale),
              (int)(NESVideoHeight * e->screen_scale));
}

void emulator_set_video_scale(Emulator *e, float scale)
{
    e->screen_scale = scale;
    printf("Scale: %.2f set. Screen: %dx%d",
              e->screen_scale,
              (int)(NESVideoWidth  * e->screen_scale),
              (int)(NESVideoHeight * e->screen_scale));
}



/* Tasti (raylib: usa KEY_*) */
/*void emulator_set_keys(Emulator *e,
                       const int *p1_keys, int p1_count,
                       const int *p2_keys, int p2_count)
{
    controller_set_bindings(e->controller_set, p1_keys, p1_count, p2_keys, p2_count);
}*/

/* Audio mute */
void emulator_mute_audio(Emulator *emu, bool mute)
{
    audio_mute(emu->audio_player);
}
