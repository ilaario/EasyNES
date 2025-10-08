//
// Created by Dario Bonfiglio on 10/9/25.
//

#include "headers/cartridge.h"

FILE* cartridge_pointer;

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

/**
 * Read the first 4 bytes from the header [0x00000 ... 0x00003] and validate the signature.
 * The signature is read in Little Endian and it represent the string "NES\0x1A"
 * @param buf The raw header used to read the signature
 * @param size Size of the raw header, if it's less than 4 bytes the signature is already invalid
 * @return 'true' if the signature read is equal to 0x1A53454E, else 'false' if it's not equal.
 */
bool valid_signature(const uint8_t buf[], size_t size){
    if(size < 4) return false;
    uint32_t sig = (uint32_t)buf[0] |
                   (uint32_t)buf[1] << 8  |
                   (uint32_t)buf[2] << 16 |
                   (uint32_t)buf[3] << 24;
    return (sig == (uint32_t)0x1A53454E);
}

/**
 * Return the string representing the Mirror type in Flag 6 of the header
 * @param mirror the enum value
 * @return A string, representing the type of mirroring
 */
const char* get_mirroring(enum mirror_type mirror)
{
    switch (mirror)
    {
        case 0: return "MIRROR_VERTICAL";
        case 1: return "MIRROR_HORIZONTAL";
        case 2: return "MIRROR_FOUR_SCREEN";
    }
}

/**
 * Return the string representing the Format type in Flag 7 of the header
 * @param format the enum value
 * @return A string, representing the format type
 */
const char* get_format(enum nes_rom_format format)
{
    switch (format)
    {
        case 0: return "NES_FORMAT_INES1";
        case 1: return "NES_FORMAT_NES20";
        case 2: return "NES_FORMAT_UNKNOWN";
    }
}

/**
 * Return the string representing the type in Flag 7 of the header
 * @param format the enum value
 * @return A string, representing the type
 */
const char* get_type(enum vs_playchoice type)
{
    switch (type)
    {
        case 0: return "NES_STANDARD";
        case 1: return "VS_SYSTEM";
        case 2: return "PLAYCHOICE";
    }
}

/**
 * Read the .nes file and allocate the amount of memory required in the header
 * @param cartridge_path The path pointing the .nes file
 * @return a Cartridge struct
 */
cartrige read_allocate_cartridge(const char* cartridge_path){
    cartridge_pointer = (FILE*)fopen(cartridge_path, "rb");
    if(cartridge_pointer == NULL){
        perror("!Error! - Invalid read");
        exit(EXIT_FAILURE);
    }

    printf("NES cartridge file loaded\nReading header file...\n");

    cartrige pCartridge = (cartrige)malloc(sizeof(struct Cartridge) + 1);
    if(pCartridge == NULL) {
        perror("!Error! - Invalid malloc");
        exit(EXIT_FAILURE);
    }

    size_t read_bytes = fread(pCartridge -> header.raw_header, 1, 16, cartridge_pointer);
    printf("Byte header letti = %zu\n", read_bytes);

    printf("Validating signature...\n");
    pCartridge -> header.valid_signature = valid_signature(pCartridge -> header.raw_header,
                                                           sizeof(pCartridge -> header.raw_header) / sizeof(uint8_t));
    if(!pCartridge -> header.valid_signature) printf("!Warning! Invalid signature\n");
    else printf("Found valid signature\n");

    printf("Reading PRG-ROM size...\n");
    pCartridge -> header.prg_rom_size_bytes = pCartridge -> header.raw_header[4] * 16;
    if(pCartridge -> header.prg_rom_size_bytes <= 0) {
        perror("!Error! Invalid PRG-ROM read");
        exit(EXIT_FAILURE);
    }
    printf("Read %dKiB of PRG-ROM (%d Slots * 16KiB)\n", pCartridge -> header.prg_rom_size_bytes, pCartridge -> header.raw_header[4]);

    printf("Reading CHR-ROM size...");
    pCartridge -> header.chr_rom_size_bytes = pCartridge -> header.raw_header[5] * 8;
    if(pCartridge -> header.chr_rom_size_bytes == 0){
        printf("No CHR-ROM found.\n");
        pCartridge -> chr_rom = NULL;
    } else {
        printf("Read %dKiB of CHR-ROM (%d Slots * 8KiB)\n", pCartridge -> header.chr_rom_size_bytes, pCartridge -> header.raw_header[5]);
    }

    printf("Reading Flag 6 byte...\n");

    pCartridge -> header.flags6 = pCartridge -> header.raw_header[6];

    bool ver_flip                       = (pCartridge -> header.flags6 & 0x01) != 0;
    bool mirror_4scr                    = (pCartridge -> header.flags6 & 0x08) != 0;
    pCartridge -> header.has_battery    = (pCartridge -> header.flags6 & 0x02) != 0;
    pCartridge -> header.has_trainer    = (pCartridge -> header.flags6 & 0x04) != 0;
    pCartridge -> header.mirroring      = ver_flip + mirror_4scr;
    pCartridge -> header.mapper_id      = (pCartridge -> header.flags6 & 0xF0) >> 4;

    if(!pCartridge -> header.has_trainer) {
        pCartridge -> trainer = NULL;
    } else {
        pCartridge -> trainer = (uint8_t*)malloc(512);
        if(pCartridge -> trainer == NULL) {
            perror("!Error! Malloc failed for Trainer");
            exit(EXIT_FAILURE);
        }
    }

    printf("Done reading Flag 6. Summary:\n"
           "Mirroring Type = %s\n"
           "Battery present = %d\n"
           "Trainer present = %d\n"
           "Mapper ID (Low Nibble) = 0x%02X",
           get_mirroring(pCartridge -> header.mirroring), pCartridge -> header.has_battery, pCartridge -> header.has_trainer, pCartridge -> header.mapper_id);

    printf("Reading Flag 7...");
    pCartridge -> header.flags7 = pCartridge -> header.raw_header[7];

    pCartridge -> header.type       = (pCartridge -> header.flags7 & 0x03);
    pCartridge -> header.format     = (pCartridge -> header.flags7 & 0x0C);
    pCartridge -> header.mapper_id  = (pCartridge -> header.flags7 & 0xF0) | pCartridge -> header.mapper_id;

    printf("Done reading Flag 7. Summary:\n"
           "Cartridge type = %s\n"
           "Cartridge format = %s\n"
           "Mapper ID (Full) = 0x%02X",
           get_type(pCartridge -> header.type), get_format(pCartridge -> header.format), pCartridge -> header.mapper_id);

    if(pCartridge -> header.format == NES_FORMAT_INES1){
        pCartridge -> header.nes2_header = NULL;
        pCartridge -> header.ines_header = (struct ines_header*)malloc(sizeof(struct ines_header));
        if(pCartridge -> header.ines_header == NULL) {
            perror("!Error! Failed allocation of iNES Header");
            exit(EXIT_FAILURE);
        }
        printf("Reading PRG-RAM size...");
        pCartridge -> header.ines_header -> prg_ram_size_bytes = (pCartridge -> header.raw_header[8] == 0 ? 8 : pCartridge -> header.raw_header[8] * 8);
        printf("Read %dKiB of PRG-RAM (%d Slots * 8KiB)",  pCartridge -> header.ines_header -> prg_ram_size_bytes, pCartridge -> header.raw_header[8]);

        printf("Reading TV System...");
        pCartridge -> header.ines_header->tv_format = pCartridge -> header.raw_header[9] & 0x01; // 0 = NTSC, 1 = PAL
        printf("Found %s TV System", pCartridge -> header.ines_header->tv_format == 0 ? "NTSC" : "PAL");

        printf("Checking padding emptiness...");
        pCartridge -> header.ines_header->padding = 0;
        for (int i = 11; i <= 15; i++){
            pCartridge -> header.ines_header->padding = (pCartridge -> header.ines_header->padding << 4) | pCartridge -> header.raw_header[i];
            // printf("?DEBUG? Padding = %d, value read = 0x%02X", pCartridge -> header.ines_header->padding, pCartridge -> header.raw_header[i]);
        }

        if(pCartridge -> header.ines_header->padding != 0) {
            perror("!Warning! Padding not empty");
        } else {
            printf("Found empty padding");
        }

    } else if (pCartridge -> header.format == NES_FORMAT_NES20){
        pCartridge -> header.ines_header = NULL;
        perror("!Warning! NES2.0 is still not supported, WIP");
        exit(EXIT_FAILURE);
    } else {
        perror("!Error! Unknown cartrige type");
        exit(EXIT_FAILURE);
    }

    printf("Allocating PRG-ROM...\n");
    pCartridge -> prg_rom = (uint8_t*)malloc(KIB(pCartridge -> header.prg_rom_size_bytes));
    if(pCartridge -> prg_rom == NULL){
        perror("!Error! Malloc failed for PRG-ROM");
        exit(EXIT_FAILURE);
    }
    printf("Allocated %dKiB for PRG-ROM\n", pCartridge -> header.prg_rom_size_bytes);

    printf("Allocating PRG-RAM...\n");
    pCartridge -> prg_ram = (uint8_t*)malloc(KIB(pCartridge -> header.ines_header -> prg_ram_size_bytes));
    if(pCartridge -> prg_ram == NULL){
        perror("!Error! Malloc failed for PRG-RAM");
        exit(EXIT_FAILURE);
    }
    printf("Allocated %dKiB for PRG-RAM\n", pCartridge -> header.ines_header -> prg_ram_size_bytes);

    if(pCartridge -> header.chr_rom_size_bytes == 0){
        pCartridge -> chr_rom = NULL;
        printf("No need to allocate CHR-ROM\nAllocating CHR-RAM instead...");
        pCartridge -> chr_ram = (uint8_t*)malloc(KIB(8));
        if(pCartridge -> chr_ram == NULL){
            perror("!Error! Malloc failed for CHR-RAM");
            exit(EXIT_FAILURE);
        }
        printf("Allocated %dKiB for CHR-RAM\n", 8);
    } else {
        pCartridge -> chr_ram = NULL;
        printf("Allocating CHR-ROM...\n");
        pCartridge -> chr_rom = (uint8_t*)malloc(KIB(pCartridge -> header.chr_rom_size_bytes));
        if(pCartridge -> chr_rom == NULL){
            perror("!Error! Malloc failed for CHR-ROM");
            exit(EXIT_FAILURE);
        }
        printf("Allocated %dKiB for CHR-ROM\n", pCartridge -> header.chr_rom_size_bytes);
    }

    uint32_t offset = 16;
    if(pCartridge -> header.has_trainer == true) {
        pCartridge -> trainer = (uint8_t*)malloc(512);
        size_t trainer_read = fread(pCartridge -> trainer, 1, 512, cartridge_pointer);
        if(trainer_read != 512){
            perror("!Error! Failed reading of trainer");
            exit(EXIT_FAILURE);
        }
        offset += 512;
    } else {
        pCartridge -> trainer = NULL;
    }

    uint32_t pgr_rom_start   = offset;
    uint32_t pgr_rom_end     = pgr_rom_start + KIB(pCartridge -> header.prg_rom_size_bytes);

    printf("Offset for PGR-ROM = (%d -> %d)", pgr_rom_start, pgr_rom_end);

    size_t prgrom_read = fread(pCartridge -> prg_rom, 1, KIB(pCartridge -> header.prg_rom_size_bytes), cartridge_pointer);
    if(prgrom_read < pCartridge -> header.prg_rom_size_bytes){
        perror("!Error! Failed reading PGR-ROM");
        exit(EXIT_FAILURE);
    }

    printf("Read and copied %dKiB from cartridge to PGR-ROM", prgrom_read / 1024);

    uint32_t chr_rom_start   = pgr_rom_end;
    uint32_t chr_rom_end     = chr_rom_start + KIB(pCartridge -> header.chr_rom_size_bytes);

    printf("Offset for CHR-ROM = (%d -> %d)", chr_rom_start, chr_rom_end);

    if(pCartridge -> header.chr_rom_size_bytes > 0){
        size_t chrrom_read = fread(pCartridge -> chr_rom, 1, KIB(pCartridge -> header.chr_rom_size_bytes), cartridge_pointer);
        if(chrrom_read < pCartridge -> header.chr_rom_size_bytes){
            perror("!Error! Failed reading PGR-ROM");
            exit(EXIT_FAILURE);
        }

        printf("Read and copied %dKiB from cartridge to CHR-ROM", chrrom_read / 1024);
    }

    rewind(cartridge_pointer);

    fseek(cartridge_pointer, 0L, SEEK_END);
    size_t sz = ftell(cartridge_pointer);

    if(chr_rom_end > sz){
        perror("!Error! Segmentation fault, read went over file size");
        exit(EXIT_FAILURE);
    }

    fclose(cartridge_pointer);

    return pCartridge;
}

/**
 * Free the struct cartridge passed by arg
 * @param pCartridge the struct to free
 */
void free_cartridge(cartrige pCartridge){
    printf("Freeing the struct...");
    free(pCartridge -> prg_rom);
    free(pCartridge -> prg_ram);
    if(pCartridge -> chr_rom != NULL) {
        free(pCartridge -> chr_rom);
    }
    if(pCartridge -> chr_ram != NULL) {
        free(pCartridge -> chr_ram);
    }
    if(pCartridge -> header.nes2_header != NULL) {
        free(pCartridge -> header.nes2_header);
    }
    if(pCartridge -> header.ines_header != NULL) {
        free(pCartridge -> header.ines_header);
    }
    if(pCartridge -> trainer != NULL) {
        free(pCartridge -> trainer);
    }
    free(pCartridge);
    printf("Struct freed");
}