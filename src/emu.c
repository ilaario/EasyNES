//
// Created by Dario Bonfiglio on 10/11/25.
//

#include "headers/emu.h"
#include "headers/apu/constants.h"

#include <raylib.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <stdio.h>

typedef uint64_t TimePointNS;

static inline TimePointNS now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (TimePointNS)ts.tv_sec * 1000000000ull + (TimePointNS)ts.tv_nsec;
}

static void pb_flush_to_gpu(p_buffer pb) {
    if (!pb) return;
    UpdateTexture(pb -> tex, pb -> pixels);
}

static void alloc_or_die(void *ptr) {
    if (!ptr) exit(EXIT_FAILURE);
}

static void free_apu(apu a) {
    if (!a) return;

    if (a -> pulse1) {
        if (a -> pulse1 -> volume) {
            free(a -> pulse1 -> volume -> divider);
            free(a -> pulse1 -> volume);
        }
        if (a -> pulse1 -> length_counter) free(a -> pulse1 -> length_counter);
        if (a -> pulse1 -> sweep) {
            free(a -> pulse1 -> sweep -> div);
            free(a -> pulse1 -> sweep);
        }
        free(a -> pulse1 -> sequencer);
        free(a -> pulse1);
        a -> pulse1 = NULL;
    }

    if (a -> pulse2) {
        if (a -> pulse2 -> volume) {
            free(a -> pulse2 -> volume -> divider);
            free(a -> pulse2 -> volume);
        }
        if (a -> pulse2 -> length_counter) free(a -> pulse2 -> length_counter);
        if (a -> pulse2 -> sweep) {
            free(a -> pulse2 -> sweep -> div);
            free(a -> pulse2 -> sweep);
        }
        free(a -> pulse2 -> sequencer);
        free(a -> pulse2);
        a -> pulse2 = NULL;
    }

    if (a -> triangle) {
        free(a -> triangle -> length);
        free(a -> triangle -> linear);
        free(a -> triangle -> sequencer);
        free(a -> triangle);
        a -> triangle = NULL;
    }

    if (a -> noise) {
        if (a -> noise -> volume) {
            free(a -> noise -> volume -> divider);
            free(a -> noise -> volume);
        }
        free(a -> noise -> length);
        free(a -> noise -> divider);
        free(a -> noise);
        a -> noise = NULL;
    }

    if (a -> dmc) {
        free(a -> dmc -> change_rate);
        free(a -> dmc);
        a -> dmc = NULL;
    }

    if (a -> frame_counter) {
        free(a -> frame_counter -> frame_slots);
        free(a -> frame_counter);
        a -> frame_counter = NULL;
    }

    free(a -> sampling_timer);
    a -> sampling_timer = NULL;
}

static bool is_truthy_env(const char *name) {
    const char *v = getenv(name);
    if (!v || !*v) return false;
    return strcmp(v, "0") != 0 && strcasecmp(v, "false") != 0 && strcasecmp(v, "off") != 0;
}

static void dump_accuracy_results_to_stream(Emulator *e, FILE *out) {
    if (!e || !e -> bus || !e -> bus -> RAM || !out) return;
    fprintf(out, "ACCURACY_DUMP_BEGIN\n");
    fprintf(out, "ACCURACY_PASSED=%u\n", (unsigned)e -> bus -> RAM[0x37]);
    for (int addr = 0x400; addr <= 0x487; ++addr) {
        uint8_t v = e -> bus -> RAM[addr & 0x7FF];
        if (v != 0) {
            fprintf(out, "ACCURACY[%03X]=%02X\n", addr, v);
        }
    }
    fprintf(out, "ACCURACY_DUMP_END\n");
}

static void write_accuracy_results_file(Emulator *e) {
    FILE *f = fopen("log/accuracy_dump.txt", "w");
    if (!f) return;
    dump_accuracy_results_to_stream(e, f);
    fclose(f);
}

static void dump_accuracy_results(Emulator *e) {
    dump_accuracy_results_to_stream(e, stdout);
    fflush(stdout);

    write_accuracy_results_file(e);
    printf("ACCURACY_DUMP_FILE=log/accuracy_dump.txt\n");
    fflush(stdout);
}

void emulator_init(Emulator *e) {
    if (!e) exit(EXIT_FAILURE);
    memset(e, 0, sizeof(*e));

    e -> cpu = (cpu)calloc(1, sizeof(struct CPU));
    e -> apu = (apu)calloc(1, sizeof(struct APU));
    e -> ppu = (ppu)calloc(1, sizeof(struct PPU));
    e -> picture_bus = (pbus)calloc(1, sizeof(struct picture_bus));
    e -> controller_set = (cs)calloc(1, sizeof(struct controller_set));
    e -> bus = (bus)calloc(1, sizeof(struct CPUBus));
    e -> audio_player = (audio_player)calloc(1, sizeof(struct AudioPlayer));
    alloc_or_die(e -> cpu);
    alloc_or_die(e -> apu);
    alloc_or_die(e -> ppu);
    alloc_or_die(e -> picture_bus);
    alloc_or_die(e -> controller_set);
    alloc_or_die(e -> bus);
    alloc_or_die(e -> audio_player);

    e -> screen_scale = 3.0f;
    e -> video_width = NESVideoWidth;
    e -> video_height = NESVideoHeight;
    InitWindow((int)(e -> video_width * e -> screen_scale),
               (int)(e -> video_height * e -> screen_scale),
               "EasyNES");
    SetTargetFPS(60);

    pbus_init(e -> picture_bus);
    create_ppu(e -> ppu, e -> picture_bus);
    controllerset_init(e -> controller_set);

    bus_init(e -> bus, e -> ppu, e -> apu, e -> controller_set, doDMA);
    cpu_init(e -> cpu, e -> bus);

    init_audio(e -> audio_player, (int)(1.0 / APU_CLOCK_PERIOD_S));
    apu_init(e -> apu,
             e -> audio_player,
             create_IRQ_handler(e -> cpu),
             create_IRQ_handler(e -> cpu),
             e -> cpu,
             emulator_dmcdma);
    setInterruptCallback(e -> ppu, nmi_interrupt);

    e -> emulator_screen = e -> ppu -> picture_buffer;

    e -> last_wakeup_ns = now_ns();
    e -> elapsed_ns = 0;
}

void emulator_dispose(Emulator *e) {
    if (!e) return;

    if (e -> mapper) {
        mapper_destroy(e -> mapper);
        e -> mapper = NULL;
    }
    if (e -> cartridge) {
        free_cartridge(e -> cartridge);
        e -> cartridge = NULL;
    }

    if (e -> ppu) {
        if (e -> ppu -> picture_buffer) {
            PBFree(e -> ppu -> picture_buffer);
            free(e -> ppu -> picture_buffer);
            e -> ppu -> picture_buffer = NULL;
        }
        if (e -> ppu -> sprite_memory) {
            free(e -> ppu -> sprite_memory -> data);
            free(e -> ppu -> sprite_memory);
            e -> ppu -> sprite_memory = NULL;
        }
        if (e -> ppu -> scanline_sprites) {
            free(e -> ppu -> scanline_sprites -> data);
            free(e -> ppu -> scanline_sprites);
            e -> ppu -> scanline_sprites = NULL;
        }
    }

    if (e -> picture_bus) {
        pbus_destroy(e -> picture_bus);
    }

    if (e -> controller_set) {
        free(e -> controller_set -> pad_1);
        free(e -> controller_set -> pad_2);
        e -> controller_set -> pad_1 = NULL;
        e -> controller_set -> pad_2 = NULL;
    }

    if (e -> bus) {
        free(e -> bus -> RAM);
        free(e -> bus -> extRAM);
        e -> bus -> RAM = NULL;
        e -> bus -> extRAM = NULL;
    }

    if (e -> audio_player) {
        spsc_ring_free(&e -> audio_player -> audio_queue);
    }

    if (e -> cpu) {
        if (e -> cpu -> irq_handlers) {
            for (int i = 0; i < e -> cpu -> irq_handlers_size; ++i) {
                free(e -> cpu -> irq_handlers[i]);
            }
            free(e -> cpu -> irq_handlers);
            e -> cpu -> irq_handlers = NULL;
        }
        e -> cpu -> irq_handlers_size = 0;
        e -> cpu -> irq_handlers_capacity = 0;
    }

    free_apu(e -> apu);

    if (IsWindowReady()) {
        CloseWindow();
    }

    free(e -> audio_player);
    free(e -> bus);
    free(e -> controller_set);
    free(e -> picture_bus);
    free(e -> ppu);
    free(e -> apu);
    free(e -> cpu);

    e -> audio_player = NULL;
    e -> bus = NULL;
    e -> controller_set = NULL;
    e -> picture_bus = NULL;
    e -> ppu = NULL;
    e -> apu = NULL;
    e -> cpu = NULL;
}

void emulator_run(Emulator *e, const char *rom_path) {
    if (!e || !rom_path) return;
    const uint64_t max_catchup_ns = CPU_CLOCK_PERIOD_NS * 50000ULL;

    e -> cartridge = read_allocate_cartridge(rom_path);
    if (!e -> cartridge) return;

    create_mapper(&e -> mapper, e -> cartridge, create_IRQ_handler(e -> cpu));
    if (!e -> mapper) {
        perror("Creating mapper failed");
        return;
    }

    if (!setMapper(e -> bus, e -> mapper) || !set_mapper(e -> picture_bus, e -> mapper)) {
        return;
    }

    cpu_reset(e -> cpu);
    reset(e -> ppu);

    e -> last_wakeup_ns = now_ns();
    e -> elapsed_ns = 0;

    bool pause = false;
    bool accuracy_debug = is_truthy_env("EASYNES_ACCURACY_DUMP");
    uint64_t frame_counter = 0;
    uint64_t post_all_screen_frames = 0;
    bool accuracy_dumped = false;
    if (accuracy_debug) {
        printf("ACCURACY_DEBUG_ENABLED=1\n");
        printf("ACCURACY_DEBUG_PATH=log/accuracy_dump.txt\n");
        fflush(stdout);
        write_accuracy_results_file(e);
    }

    while (!WindowShouldClose()) {
        if (IsKeyPressed(KEY_ESCAPE)) break;
        if (IsKeyPressed(KEY_F2)) pause = !pause;

        if (!pause) {
            TimePointNS now = now_ns();
            e -> elapsed_ns += (now - e -> last_wakeup_ns);
            e -> last_wakeup_ns = now;
            if (e -> elapsed_ns > max_catchup_ns) {
                e -> elapsed_ns = max_catchup_ns;
            }

            int cpu_budget = 50000;
            while (e -> elapsed_ns > CPU_CLOCK_PERIOD_NS && cpu_budget-- > 0) {
                step(e -> ppu, e -> cpu);
                step(e -> ppu, e -> cpu);
                step(e -> ppu, e -> cpu);
                cpu_step(e -> cpu);
                apu_step(e -> apu);
                e -> elapsed_ns -= CPU_CLOCK_PERIOD_NS;
            }
        } else {
            e -> last_wakeup_ns = now_ns();
        }

        controller_poll_host_input(e -> controller_set);

        if (accuracy_debug && e -> controller_set && e -> controller_set -> pad_1) {
            // Autostart AccuracyCoin "run all tests" from the menu.
            if (frame_counter >= 20 && frame_counter <= 160) {
                e -> controller_set -> pad_1 -> current_button |= (1u << BTN_START);
            }
        }

        if (accuracy_debug && e -> bus && e -> bus -> RAM) {
            // AccuracyCoin asm: CurrentScreen at $0036, PostAllTestScreen = 2.
            uint8_t current_screen = e -> bus -> RAM[0x36];
            if ((frame_counter % 600) == 0) {
                printf("ACCURACY_DEBUG frame=%llu screen=%u passed=%u\n",
                       (unsigned long long)frame_counter,
                       (unsigned)current_screen,
                       (unsigned)e -> bus -> RAM[0x37]);
                fflush(stdout);
                write_accuracy_results_file(e);
            }
            if (current_screen == 0x02) post_all_screen_frames++;
            else post_all_screen_frames = 0;

            if (post_all_screen_frames > 120) {
                dump_accuracy_results(e);
                accuracy_dumped = true;
                break;
            }

            if (frame_counter > (uint64_t)(60 * 120)) {
                dump_accuracy_results(e);
                accuracy_dumped = true;
                break;
            }
        }

        if (e -> emulator_screen) pb_flush_to_gpu(e -> emulator_screen);
        BeginDrawing();
        ClearBackground(BLACK);
        if (e -> emulator_screen) {
            DrawTexturePro(
                e -> emulator_screen -> tex,
                (Rectangle){0, 0, (float)e -> video_width, (float)e -> video_height},
                (Rectangle){0, 0, e -> video_width * e -> screen_scale,
                            e -> video_height * e -> screen_scale},
                (Vector2){0, 0}, 0.0f, WHITE
            );
        }
        EndDrawing();
        frame_counter++;
    }

    if (accuracy_debug && !accuracy_dumped) {
        dump_accuracy_results(e);
    }
}

void emulator_oamdma(Emulator *e, uint8_t page) {
    if (!e || !e -> cpu || !e -> bus || !e -> ppu) return;
    skip_OAM_DMA_cycles(e -> cpu);
    const uint8_t *page_ptr = getPagePtr(e -> bus, page);
    if (page_ptr) doDMA(e -> ppu, page_ptr);
}

static uint8_t dmc_dma_get_sample(cpu c, uint16_t addr, uint16_t halt_addr) {
    if (!c || !c -> bus) return 0;
    bus b = c -> bus;
    uint8_t value = bus_read(b, addr);

    // During a DMA get cycle, APU registers may be "activated" by combining
    // CPU A15-A5 (halt address) and DMA A4-A0 (DMA address).
    if ((halt_addr & 0xFFE0u) == 0x4000u) {
        uint16_t reg_addr = (uint16_t)((halt_addr & 0xFFE0u) | (addr & 0x001Fu));
        switch (reg_addr) {
            case APU_CONTROL_AND_STATUS:
                // $4015 wins conflicts and must clear frame IRQ side-effects.
                value = read_status(b -> apu);
                break;
            case JOY1:
            case JOY2_AND_FRAME_CONTROL: {
                uint8_t reg_value = bus_read(b, reg_addr);
                value &= reg_value;
                break;
            }
            default:
                break;
        }
    }

    return value;
}

uint8_t emulator_dmcdma(cpu c, uint16_t addr, uint16_t halt_addr, bool do_align_cycle, int stall_cycles) {
    if (!c || !c -> bus) return 0;
    if (stall_cycles < 0) stall_cycles = 0;
    skip_DMC_DMA_cycles(c, stall_cycles);

    (void)bus_read(c -> bus, halt_addr);
    if (do_align_cycle) (void)bus_read(c -> bus, halt_addr);

    return dmc_dma_get_sample(c, addr, halt_addr);
}

void emulator_set_video_height(Emulator *e, int height) {
    if (!e || height <= 0) return;
    e -> screen_scale = (float)height / (float)NESVideoHeight;
}

void emulator_set_video_width(Emulator *e, int width) {
    if (!e || width <= 0) return;
    e -> screen_scale = (float)width / (float)NESVideoWidth;
}

void emulator_set_video_scale(Emulator *e, float scale) {
    if (!e || scale <= 0.0f) return;
    e -> screen_scale = scale;
}

void emulator_mute_audio(Emulator *emu, bool mute) {
    if (!emu || !emu -> audio_player) return;
    audio_mute(emu -> audio_player, mute);
}
