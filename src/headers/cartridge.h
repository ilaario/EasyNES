//
// Created by ilaario on 10/4/25.
//

#ifndef EASYNES_CARTRIDGE_H
#define EASYNES_CARTRIDGE_H

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

typedef struct Cartridge* cartrige;

#endif //EASYNES_CARTRIDGE_H
