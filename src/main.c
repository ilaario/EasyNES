#include "headers/main.h"
#include "headers/ppu.h"

#ifdef LOG_DEBUG
#undef LOG_DEBUG
#endif
#ifdef LOG_INFO
#undef LOG_INFO
#endif
#ifdef LOG_WARNING
#undef LOG_WARNING
#endif

#include <raylib.h>
#include <math.h>      // fminf, floorf
#include <stdlib.h>
#include <stdio.h>

cartridge cart;

static void rl_panic(const char *what) {
    fprintf(stderr, "Raylib error in %s\n", what);
    exit(EXIT_FAILURE);
}

int main(int argc, char const *argv[]) {
    log_init("log/easynes.log");

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <game>.nes\n", argc > 0 ? argv[0] : "easynes");
        log_stop();
        return EXIT_FAILURE;
    }

    // ---- Core init ----
    printf("Init...\n");
    bus bus1 = (bus)malloc(sizeof(struct CPUBus));
    if (!bus1) { fprintf(stderr, "Out of memory (bus)\n"); exit(EXIT_FAILURE); }

    ppu ppu1 = (ppu)malloc(sizeof(struct PPU));
    if (!ppu1) { fprintf(stderr, "Out of memory (ppu)\n"); exit(EXIT_FAILURE); }

    // cart = make_dummy(16, 8, false, false);
    // ---- Cartridge ----
    cart = read_allocate_cartridge(argv[1]);
    print_info(cart);
    printf("Cartridge read - DONE\n");


    mapper mapper1 = create_mapper_for_cart(cart);
    controller pad1 = NULL;
    controller pad2 = NULL;

    cpu cpu1 = create_cpu(bus1);

    ppu_init(ppu1, mapper1, cart->header.mirroring);
    bus_init(bus1, ppu1, mapper1, pad1, pad2);

    cpu_reset(cpu1);
    ppu_reset(ppu1);
    bus_reset(bus1);

    printf("Init ended\n");

    // ---- Tests ----
    //test_setup(cpu1, bus1, ppu1, mapper1, pad1, pad2);
    //run_all_tests();

    // =====================================================
    //                 RAYLIB VISUALIZATION LOOP
    // =====================================================

    printf("Starting raylib visualizer...\n");

    const int NES_W = 256;
    const int NES_H = 240;

    // Framebuffer CPU-side (RGBA8888, tightly packed)
    Color *frame = (Color*)MemAlloc((size_t)NES_W * (size_t)NES_H * sizeof(Color));
    if (!frame) { fprintf(stderr, "Out of memory (framebuffer)\n"); exit(EXIT_FAILURE); }

    // Re-init core per non sporcare coi test
    cpu_reset(cpu1);
    ppu_reset(ppu1);
    bus_reset(bus1);

    // Finestra
    char title[256];
    snprintf(title, sizeof(title), "EasyNES %s", GetFileNameWithoutExt(argv[1]));
    // High-DPI aware, resizable window
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(NES_W * 3, NES_H * 3, title);
    SetWindowMinSize(NES_W, NES_H);
    SetTargetFPS(60);

    // Creiamo una texture da un Image che punta al nostro buffer
    Image img = {
            .data = frame,
            .width = NES_W,
            .height = NES_H,
            .mipmaps = 1,
            .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8
    };
    Texture2D tex = LoadTextureFromImage(img);
    if (tex.id == 0) rl_panic("LoadTextureFromImage");

    SetTextureFilter(tex, TEXTURE_FILTER_POINT);
    SetTextureWrap(tex, TEXTURE_WRAP_CLAMP);

    // Loop
    while (!WindowShouldClose()) {
        // Avanza l’emulazione di 1 frame (implementazione tua)
        emu_run_for_frames(cpu1, bus1, 1);

        // Disegna il frame reale dalla PPU (RGBA8888)
        ppu_get_frame_rgba(ppu1, (uint32_t*)frame);

        // Aggiorna la texture dai nostri pixel (pitch = NES_W * 4)
        UpdateTexture(tex, frame);

        // Scaling intero + centrato (usa dimensioni LOGICHE della finestra)
        int winW = GetScreenWidth();
        int winH = GetScreenHeight();

        float sx = (float)winW / (float)NES_W;
        float sy = (float)winH / (float)NES_H;
        float s  = floorf(fminf(sx, sy));
        if (s < 1.0f) s = 1.0f;

        float drawW = (float)NES_W * s;
        float drawH = (float)NES_H * s;
        float offX  = ((float)winW - drawW) * 0.5f;
        float offY  = ((float)winH - drawH) * 0.5f;

        Rectangle src = (Rectangle){ 0, 0, (float)NES_W, (float)NES_H };           // niente flip
        Rectangle dst = (Rectangle){ offX, offY, drawW, drawH };

        BeginDrawing();
        ClearBackground((Color){16,16,16,255});
        DrawTexturePro(tex, src, dst, (Vector2){0,0}, 0.0f, WHITE);
        DrawFPS(14, 10);
        EndDrawing();
    }

    // ---- Cleanup raylib ----
    UnloadTexture(tex);
    CloseWindow();
    printf("raylib visualizer closed.\n");

    // ---- Cleanup core ----
    MemFree(frame);
    free_cartridge(cart);
    mapper_destroy(mapper1);
    free(bus1);
    free(ppu1);

    log_stop();
    return 0;
}