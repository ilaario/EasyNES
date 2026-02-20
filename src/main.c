#include "headers/main.h"

int main(int argc, char const *argv[]) {
    log_init("log/easynes.log");

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <game>.nes\n", argc > 0 ? argv[0] : "easynes");
        log_stop();
        return EXIT_FAILURE;
    }

    Emulator emu;
    emulator_init(&emu);
    emulator_run(&emu, argv[1]);
    emulator_dispose(&emu);

    log_stop();
    return EXIT_SUCCESS;
}
