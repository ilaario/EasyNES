//
// Created by ilaario on 10/4/25.
//

#ifndef EASYNES_CARTRIDGE_H
#define EASYNES_CARTRIDGE_H

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "logger.h"

#define ANSI_RED   "\x1b[31m"
#define ANSI_RESET "\x1b[0m"

#define KIB(x) ((x) * 1024)

#ifdef DEBUGLOG
#define perror(fmt, ...)                                                       \
    do {                                                                       \
        struct timeval tv;                                                     \
        gettimeofday(&tv, NULL);                                               \
                                                                               \
        struct tm tm_info;                                                     \
        localtime_r(&tv.tv_sec, &tm_info);                                     \
                                                                               \
        char ts[64];                                                           \
        size_t ts_len = strftime(ts, sizeof(ts),                               \
                                 "%d/%m/%Y - %H:%M:%S", &tm_info);             \
        ts_len += snprintf(ts + ts_len, sizeof(ts) - ts_len,                   \
                           ".%03d", (int)(tv.tv_usec / 1000));                 \
                                                                               \
        fprintf(stdout, ANSI_RED                                               \
                "[%s] - [%s:%d] - [PID:%d] - [%s] --> [" fmt "]\n" ANSI_RESET, \
                "SYSER",                                                       \
                __FILE__,__LINE__,                                             \
                (int)getpid(),                                                 \
                ts,                                                            \
                ##__VA_ARGS__);                                                \
    } while (0)

#else
#define perror(buffer, ...) syserr_log(buffer, ##__VA_ARGS__)
#endif

#ifdef DEBUGLOG
#define printf(fmt, ...)                                                       \
    do {                                                                       \
        struct timeval tv;                                                     \
        gettimeofday(&tv, NULL);                                               \
                                                                               \
        struct tm tm_info;                                                     \
        localtime_r(&tv.tv_sec, &tm_info);                                     \
                                                                               \
        char ts[64];                                                           \
        size_t ts_len = strftime(ts, sizeof(ts),                               \
                                 "%d/%m/%Y - %H:%M:%S", &tm_info);             \
        ts_len += snprintf(ts + ts_len, sizeof(ts) - ts_len,                   \
                           ".%03d", (int)(tv.tv_usec / 1000));                 \
                                                                               \
        fprintf(stdout,                                                        \
                "[%s] - [%s:%d] - [PID:%d] - [%s] --> [" fmt "]\n",            \
                "TRACE",                                                       \
                __FILE__, __LINE__,                                            \
                (int)getpid(),                                                 \
                ts,                                                            \
                ##__VA_ARGS__);                                                \
    } while (0)

#else
#define printf(buffer, ...) appinfo_log(buffer, ##__VA_ARGS__)
#endif

#ifdef DEBUGLOG
#define log_init(filePath) printf("DEBUG LOG STARTED AT %s", filePath)
#else
#define log_init(filePath) log_init(filePath)
#endif

#ifdef DEBUGLOG
#define log_stop() printf("DEBUG LOG STOPPED")
#else
#define log_stop() log_stop()
#endif

// file format, could be INES1 or NES2.0
enum nes_rom_format {
    NES_FORMAT_INES1 = 0,
    NES_FORMAT_NES20 = 1,
    NES_FORMAT_UNKNOWN = 2
};

enum mirror_type {
    MIRROR_VERTICAL,
    MIRROR_HORIZONTAL,
    MIRROR_FOUR_SCREEN
};

enum vs_playchoice {
    NES_STANDARD,
    VS_SYSTEM,
    PLAYCHOICE
};

struct ines_header{
    uint32_t prg_ram_size_bytes;    // 0 => default 8KiB
    uint8_t tv_format;              // 0 = NTSC, 1 = PAL
    uint8_t playchoice10_flag;      // In alcune ROM 0 = nessuno, 1 = PlayChoice-10.
    int padding;                    // Dovrebbe essere vuoto, se contiene qualcosa, lettura sbagliata della cartuccia
};

struct nes2_header{
    uint8_t flag8;
    uint8_t flag9;
    uint8_t flag10;
    uint8_t flag11;

    uint8_t submapper_id;

    uint8_t prg_ram_shift;
    uint8_t prg_nvram_shift;

    uint8_t chr_ram_shift;
    uint8_t chr_nvram_shift;
};

struct cartridge_header {
    // contains the raw header readed from the cartrige file
    uint8_t raw_header[16];
    struct nes2_header* nes2_header;
    struct ines_header* ines_header;

    // signature
    bool valid_signature;           // "NES\x1A"
    enum nes_rom_format format;     // iNES or NES2.0
    enum vs_playchoice type;        // 00 = NES standard, 01 = VS System, 10 = PlayChoice-10

    // required fields for INES1 cartrige types
    uint8_t flags6;                 // header[6]
    uint8_t flags7;                 // header[7]

    // other fields
    uint32_t prg_rom_size_bytes;    // units * 16KiB
    uint32_t chr_rom_size_bytes;    // units * 8KiB

    bool has_trainer;               // flags6 bit2
    bool has_battery;               // flags6 bit1
    enum mirror_type mirroring;     // orizz/vert/4-screen
    uint16_t mapper_id;             // high nibble flags7 + low nibble flags6

};

struct Cartridge {
    struct cartridge_header header;   // per debug/diagnostica

    // Dati ROM caricati dal file (ownership tua)
    uint8_t* prg_rom;    // size = meta.prg_rom_size
    uint8_t* chr_rom;    // size = meta.chr_rom_size (NULL se uses_chr_ram)
    uint8_t* trainer;    // size = 512 se presente, altrimenti NULL

    // RAM allocate dall’emulatore
    uint8_t* prg_ram;    // size = meta.prg_ram_size (anche 0 → NULL)
    uint8_t* chr_ram;    // size = meta.chr_ram_size se uses_chr_ram, else NULL

    // Stato mapper (per NROM è nullo; per altri mapper serviranno registri)
    void* mapper_state;   // oppure una union/struct specifica per mapper 0/1/4…
};

typedef struct Cartridge* cartridge;

/**
 * Read the .nes file and allocate the amount of memory required in the header
 * @param cartridge_path The path pointing the .nes file
 * @return a Cartridge struct
 */
cartridge read_allocate_cartridge(const char* cartridge_path);

/**
 * Free the struct cartridge passed by arg
 * @param pCartridge the struct to free
 */
void free_cartridge(cartridge pCartridge);

/**
 * Print a log of the cartridge passed by arg
 * @param pCartridge a Cartridge struct
 */
void print_info(cartridge pCartridge);

/**
 * Create a dummy cartridge for testing purpose
 * @param prg_kib Value of KiB to allocate for PRG-ROM
 * @param chr_kib Value of KiB to allocate for CHR-ROM/RAM
 * @param has_prg_ram Indicate if the Cartridge have PRG-RAM
 * @param chr_is_ram Indicate if the CHR is RAM or ROM
 * @return A pointer to the dummy cartridge
 */
cartridge make_dummy(uint8_t prg_kib, uint8_t chr_kib, bool has_prg_ram, bool chr_is_ram);

#endif //EASYNES_CARTRIDGE_H
