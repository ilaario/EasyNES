#include "headers/main.h"

cartrige pCartridge;

int main(int argc, char const *argv[]) {
    log_init("log/easynes.log");

    if (argc != 2) {
        fprintf(stderr, "Usage: <game>.nes\n");
        return EXIT_FAILURE;
    }

    pCartridge = read_allocate_cartridge(argv[1]);

    print_info(pCartridge);

    free_cartridge(pCartridge);
    log_stop();
    return 0;
}
