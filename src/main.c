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


    printf("Init ended\n");

    // ---- Tests ----
    //test_setup(cpu1, bus1, ppu1, mapper1, pad1, pad2);
    //run_all_tests();

    // =====================================================
    //                 RAYLIB VISUALIZATION LOOP
    // =====================================================

    printf("Starting raylib visualizer...\n");

    log_stop();
    return 0;
}