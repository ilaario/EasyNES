//
// Created by ilaario on 10/4/25.
//

#ifndef EASYNES_CARTRIGE_H
#define EASYNES_CARTRIGE_H

// file format, could be INES1 or NES2.0
enum nes_rom_format {
    NES_FORMAT_INES1,
    NES_FORMAT_NES20,
    NES_FORMAT_UNKNOWN
};

enum mirror_type {
    MIRROR_VERTICAL,
    MIRROR_HORIZONTAL,
    MIRROR_FOUR_SCREEN
};

struct cartrige_header {
    // contains the raw header readed from the cartrige file
    uint8_t raw_header[16];

    // signature
    bool valid_signature;   // "NES\x1A"
    enum nes_rom_format format;

    // required fields for INES1 cartrige types
    uint8_t prg_rom_units_16k;  // header[4]
    uint8_t chr_rom_units_8k;   // header[5]
    uint8_t flags6;             // header[6]
    uint8_t flags7;             // header[7]
    uint8_t prg_ram_units_8k;   // header[8] (iNES1)

    // other fields
    uint32_t prg_rom_size_bytes;  // units * 16KiB
    uint32_t chr_rom_size_bytes;  // units * 8KiB
    uint32_t prg_ram_size_bytes;  // 0 => default 8KiB (iNES1)

    bool has_trainer;         // flags6 bit2
    bool has_battery;         // flags6 bit1
    enum mirror_type mirroring;// orizz/vert/4-screen
    uint16_t mapper_id;         // high nibble flags7 + low nibble flags6
};

struct card_meta {
    uint16_t mapper_id;
    enum mirror_type mirroring;
    bool battery_present;

    // Taglie finali decise per l’emulatore
    uint32_t prg_rom_size;
    uint32_t chr_rom_size;  // 0 se userai CHR-RAM
    uint32_t prg_ram_size;  // applicando default se servono
    uint32_t chr_ram_size;  // es. 8KiB se non c’è CHR-ROM

    bool has_trainer;
    bool uses_chr_ram;      // true se chr_rom_size == 0
};

struct Cartridge {
    struct cartrige_header header;   // per debug/diagnostica
    struct card_meta  meta;     // comodo per l’emulatore

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

#endif //EASYNES_CARTRIGE_H
