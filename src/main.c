#include "headers/main.h"

FILE* cartridge_pointer;
cartrige pCartridge;

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

/*
   function load_cartridge(path) -> Cartridge or error:
    file = read_all(path)
    if size(file) < 16: error

    hdr = parse_ines_header(file[0..15])

    meta = derive_meta_from_header(hdr)

    cursor = 16
    if meta.has_trainer:
        trainer = copy(file[cursor .. cursor+512]); cursor += 512
    else:
        trainer = NULL

    if size(file) < cursor + meta.prg_rom_size + meta.chr_rom_size: error

    prg_rom = copy(file[cursor .. cursor+meta.prg_rom_size])
    cursor += meta.prg_rom_size

    if meta.chr_rom_size > 0:
        chr_rom = copy(file[cursor .. cursor+meta.chr_rom_size])
        cursor += meta.chr_rom_size
        chr_ram = NULL
    else:
        chr_rom = NULL
        chr_ram = alloc(meta.chr_ram_size) // inizializza a 0

    // PRG-RAM
    prg_ram = (meta.prg_ram_size > 0) ? alloc(meta.prg_ram_size) : NULL

    return Cartridge{
        header = hdr,
        meta = meta,
        prg_rom = prg_rom,
        chr_rom = chr_rom,
        trainer = trainer,
        prg_ram = prg_ram,
        chr_ram = chr_ram,
        mapper_state = NULL
    }
 */
cartrige read_allocate_cartridge(char* cartridge_path){
    cartridge_pointer = (FILE*)fopen(cartridge_path, "rb");
    if(cartridge_pointer == NULL){
        perror("!Error! - Invalid read");
        exit(EXIT_FAILURE);
    }

    printf("NES cartridge file loaded\nReading header file...\n");

    pCartridge = (cartrige)malloc(sizeof(struct Cartridge) + 1);
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
    printf("Allocating PRG-ROM...\n");
    pCartridge -> prg_rom = (uint8_t*)malloc(KIB(pCartridge -> header.prg_rom_size_bytes));
    if(pCartridge -> prg_rom == NULL){
        perror("!Error! Malloc failed for PRG-ROM");
        exit(EXIT_FAILURE);
    }
    printf("Allocated %dKiB for PRG-ROM\n", pCartridge -> header.prg_rom_size_bytes);

    printf("Reading CHR-ROM size...");
    pCartridge -> header.chr_rom_size_bytes = pCartridge -> header.raw_header[5] * 8;
    if(pCartridge -> header.chr_rom_size_bytes == 0){
        printf("No CHR-ROM found.\n");
        pCartridge -> chr_rom = NULL;
    } else {
        printf("Read %dKiB of CHR-ROM (%d Slots * 8KiB)\n", pCartridge -> header.chr_rom_size_bytes, pCartridge -> header.raw_header[5]);
        printf("Allocating CHR-ROM...\n");
        pCartridge -> chr_rom = (uint8_t*)malloc(KIB(pCartridge -> header.chr_rom_size_bytes));
        if(pCartridge -> chr_rom == NULL){
            perror("!Error! Malloc failed for CHR-ROM");
            exit(EXIT_FAILURE);
        }
        printf("Allocated %dKiB for CHR-ROM\n", pCartridge -> header.chr_rom_size_bytes);
    }

    printf("Reading Flag 6 byte...\n");

    pCartridge -> header.flags6 = pCartridge -> header.raw_header[6];

    bool ver_flip                       = (pCartridge -> header.flags6 & 0x01) != 0;
    bool mirror_4scr                    = (pCartridge -> header.flags6 & 0x08) != 0;
    pCartridge -> header.has_battery    = (pCartridge -> header.flags6 & 0x02) != 0;
    pCartridge -> header.has_trainer    = (pCartridge -> header.flags6 & 0x04) != 0;
    pCartridge -> header.mirroring      = ver_flip + mirror_4scr;

    pCartridge -> header.mapper_id       = (pCartridge -> header.flags6 & 0xF0) >> 4;

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
           "Mapper ID = 0x%02X",
           get_mirroring(pCartridge -> header.mirroring), pCartridge -> header.has_battery, pCartridge -> header.has_trainer, pCartridge -> header.mapper_id);

    return pCartridge;
}

int main() {
    log_init("log/easynes.log");
    pCartridge = read_allocate_cartridge("test/The_Legend_of_Zelda.nes");

    free(pCartridge -> prg_rom);
    free(pCartridge);
    log_stop();
    return 0;
}
