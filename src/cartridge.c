//
// Created by Dario Bonfiglio on 10/9/25.
//

#include "headers/cartridge.h"

FILE* cartridge_pointer;

static uint64_t nes2_shift_to_bytes(uint8_t shift) {
    if (shift == 0) return 0;
    if (shift >= 58) return 0;
    return 64ull << shift;
}

static uint64_t nes2_exp_mult_to_bytes(uint8_t encoded) {
    uint8_t multiplier = (uint8_t)((encoded & 0x03u) * 2u + 1u);
    uint8_t exponent = (uint8_t)(encoded >> 2u);
    if (exponent >= 63) return 0;
    return ((uint64_t)multiplier) << exponent;
}

static uint32_t bytes_to_kib_ceil(uint64_t bytes) {
    if (bytes == 0) return 0;
    uint64_t kib = (bytes + 1023ull) / 1024ull;
    if (kib > UINT32_MAX) return 0;
    return (uint32_t)kib;
}

static bool header_tail_is_zero(const uint8_t raw_header[16]) {
    for (int i = 12; i <= 15; ++i) {
        if (raw_header[i] != 0) return false;
    }
    return true;
}

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
        case 2: return "ONE_LOWER_SCREEN";
        case 3:return "ONE_SCREEN_HIGHER";
        case 4: return "MIRROR_FOUR_SCREEN";
    }
    return "MIRROR_UNKNOWN";
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
    return "NES_FORMAT_UNKNOWN";
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
    return "TYPE_UNKNOWN";
}

/**
 * Read the .nes file and allocate the amount of memory required in the header
 * @param cartridge_path The path pointing the .nes file
 * @return a Cartridge struct
 */
cartridge read_allocate_cartridge(const char* cartridge_path){
    cartridge_pointer = (FILE*)fopen(cartridge_path, "rb");
    if(cartridge_pointer == NULL){
        perror("!Error! - Invalid read");
        exit(EXIT_FAILURE);
    }

    printf("NES cartridge file loaded\nReading header file...\n");

    cartridge pCartridge = (cartridge)calloc(1, sizeof(struct Cartridge));
    if(pCartridge == NULL) {
        perror("!Error! - Invalid malloc");
        exit(EXIT_FAILURE);
    }

    size_t read_bytes = fread(pCartridge -> header.raw_header, 1, 16, cartridge_pointer);
    printf("Byte header letti = %zu\n", read_bytes);
    if (read_bytes != 16) {
        perror("!Error! Header size is invalid (%zu bytes)", read_bytes);
        exit(EXIT_FAILURE);
    }

    printf("Validating signature...\n");
    pCartridge -> header.valid_signature = valid_signature(pCartridge -> header.raw_header,
                                                           sizeof(pCartridge -> header.raw_header) / sizeof(uint8_t));
    if(!pCartridge -> header.valid_signature) {
        perror("!Error! Invalid signature");
        exit(EXIT_FAILURE);
    }
    printf("Found valid signature\n");

    printf("Reading Flag 6 byte...\n");

    pCartridge -> header.flags6 = pCartridge -> header.raw_header[6];
    pCartridge -> header.flags7 = pCartridge -> header.raw_header[7];

    bool ver_flip                       = (pCartridge -> header.flags6 & 0x01) != 0;
    bool mirror_4scr                    = (pCartridge -> header.flags6 & 0x08) != 0;
    pCartridge -> header.has_battery    = (pCartridge -> header.flags6 & 0x02) != 0;
    pCartridge -> header.has_trainer    = (pCartridge -> header.flags6 & 0x04) != 0;
    if (mirror_4scr) pCartridge -> header.mirroring = FOUR_SCREEN;
    else pCartridge -> header.mirroring = ver_flip ? MIRROR_VERTICAL : MIRROR_HORIZONTAL;

    printf("Done reading Flag 6. Summary:\n"
           "Mirroring Type = %s\n"
           "Battery present = %d\n"
           "Trainer present = %d\n"
           "Mapper ID (Low Nibble) = 0x%02X",
           get_mirroring(pCartridge -> header.mirroring),
           pCartridge -> header.has_battery,
           pCartridge -> header.has_trainer,
           (pCartridge -> header.flags6 & 0xF0) >> 4);

    printf("Reading Flag 7...");
    pCartridge -> header.type       = (enum vs_playchoice)(pCartridge -> header.flags7 & 0x03);
    uint8_t format_bits             = (uint8_t)((pCartridge -> header.flags7 >> 2) & 0x03);
    pCartridge -> header.format     = (format_bits == 0x02) ? NES_FORMAT_NES20 : NES_FORMAT_INES1;
    if (format_bits == 0x01 || format_bits == 0x03) {
        perror("!Warning! Non-standard format bits (%u). Falling back to iNES parsing", format_bits);
    }

    uint16_t mapper_low = (uint16_t)((pCartridge -> header.flags6 & 0xF0) >> 4);
    uint16_t mapper_mid = (uint16_t)(pCartridge -> header.flags7 & 0xF0);
    if (pCartridge -> header.format == NES_FORMAT_INES1 && !header_tail_is_zero(pCartridge -> header.raw_header)) {
        mapper_mid = 0;
    }
    pCartridge -> header.mapper_id = (uint16_t)(mapper_low | mapper_mid);

    printf("Done reading Flag 7. Summary:\n"
           "Cartridge type = %s\n"
           "Cartridge format = %s\n"
           "Mapper ID (Full) = 0x%02X",
           get_type(pCartridge -> header.type), get_format(pCartridge -> header.format), pCartridge -> header.mapper_id);

    uint32_t prg_ram_kib = 0;
    uint32_t chr_ram_kib = 8;

    if(pCartridge -> header.format == NES_FORMAT_INES1){
        pCartridge -> header.nes2_header = NULL;
        pCartridge -> header.ines_header = (struct ines_header*)malloc(sizeof(struct ines_header));
        if(pCartridge -> header.ines_header == NULL) {
            perror("!Error! Failed allocation of iNES Header");
            exit(EXIT_FAILURE);
        }
        pCartridge -> header.prg_rom_size_bytes = (uint32_t)pCartridge -> header.raw_header[4] * 16u;
        pCartridge -> header.chr_rom_size_bytes = (uint32_t)pCartridge -> header.raw_header[5] * 8u;
        if(pCartridge -> header.prg_rom_size_bytes == 0) {
            perror("!Error! Invalid PRG-ROM read");
            exit(EXIT_FAILURE);
        }
        printf("Read %uKiB of PRG-ROM (%u Slots * 16KiB)\n",
               pCartridge -> header.prg_rom_size_bytes,
               pCartridge -> header.raw_header[4]);
        if(pCartridge -> header.chr_rom_size_bytes == 0){
            printf("No CHR-ROM found.\n");
        } else {
            printf("Read %uKiB of CHR-ROM (%u Slots * 8KiB)\n",
                   pCartridge -> header.chr_rom_size_bytes,
                   pCartridge -> header.raw_header[5]);
        }

        printf("Reading PRG-RAM size...");
        pCartridge -> header.ines_header -> prg_ram_size_bytes = (pCartridge -> header.raw_header[8] == 0 ? 8 : pCartridge -> header.raw_header[8] * 8);
        prg_ram_kib = pCartridge -> header.ines_header -> prg_ram_size_bytes;
        printf("Read %uKiB of PRG-RAM (%u Slots * 8KiB)",
               pCartridge -> header.ines_header -> prg_ram_size_bytes,
               pCartridge -> header.raw_header[8]);

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
        pCartridge -> header.nes2_header = (struct nes2_header*)calloc(1, sizeof(struct nes2_header));
        pCartridge -> header.ines_header = (struct ines_header*)calloc(1, sizeof(struct ines_header));
        if (!pCartridge -> header.nes2_header || !pCartridge -> header.ines_header) {
            perror("!Error! Failed allocation of NES2.0 headers");
            exit(EXIT_FAILURE);
        }

        pCartridge -> header.nes2_header -> flag8 = pCartridge -> header.raw_header[8];
        pCartridge -> header.nes2_header -> flag9 = pCartridge -> header.raw_header[9];
        pCartridge -> header.nes2_header -> flag10 = pCartridge -> header.raw_header[10];
        pCartridge -> header.nes2_header -> flag11 = pCartridge -> header.raw_header[11];
        pCartridge -> header.nes2_header -> submapper_id = (uint8_t)(pCartridge -> header.raw_header[8] >> 4);
        pCartridge -> header.nes2_header -> prg_ram_shift = (uint8_t)(pCartridge -> header.raw_header[10] & 0x0F);
        pCartridge -> header.nes2_header -> prg_nvram_shift = (uint8_t)((pCartridge -> header.raw_header[10] >> 4) & 0x0F);
        pCartridge -> header.nes2_header -> chr_ram_shift = (uint8_t)(pCartridge -> header.raw_header[11] & 0x0F);
        pCartridge -> header.nes2_header -> chr_nvram_shift = (uint8_t)((pCartridge -> header.raw_header[11] >> 4) & 0x0F);

        uint8_t prg_msb = (uint8_t)(pCartridge -> header.raw_header[9] & 0x0F);
        uint8_t chr_msb = (uint8_t)((pCartridge -> header.raw_header[9] >> 4) & 0x0F);

        if (prg_msb != 0x0F) {
            pCartridge -> header.prg_rom_size_bytes =
                (((uint32_t)prg_msb << 8) | pCartridge -> header.raw_header[4]) * 16u;
        } else {
            pCartridge -> header.prg_rom_size_bytes =
                bytes_to_kib_ceil(nes2_exp_mult_to_bytes(pCartridge -> header.raw_header[4]));
        }

        if (chr_msb != 0x0F) {
            pCartridge -> header.chr_rom_size_bytes =
                (((uint32_t)chr_msb << 8) | pCartridge -> header.raw_header[5]) * 8u;
        } else {
            pCartridge -> header.chr_rom_size_bytes =
                bytes_to_kib_ceil(nes2_exp_mult_to_bytes(pCartridge -> header.raw_header[5]));
        }

        if (pCartridge -> header.prg_rom_size_bytes == 0) {
            perror("!Error! Invalid NES2.0 PRG-ROM size");
            exit(EXIT_FAILURE);
        }

        uint16_t mapper_high = (uint16_t)(pCartridge -> header.raw_header[8] & 0x0F);
        pCartridge -> header.mapper_id |= (uint16_t)(mapper_high << 8);

        uint64_t prg_ram_bytes = nes2_shift_to_bytes(pCartridge -> header.nes2_header -> prg_ram_shift);
        uint64_t prg_nvram_bytes = nes2_shift_to_bytes(pCartridge -> header.nes2_header -> prg_nvram_shift);
        uint64_t chr_ram_bytes = nes2_shift_to_bytes(pCartridge -> header.nes2_header -> chr_ram_shift);
        uint64_t chr_nvram_bytes = nes2_shift_to_bytes(pCartridge -> header.nes2_header -> chr_nvram_shift);

        prg_ram_kib = bytes_to_kib_ceil(prg_ram_bytes > prg_nvram_bytes ? prg_ram_bytes : prg_nvram_bytes);
        chr_ram_kib = bytes_to_kib_ceil(chr_ram_bytes > chr_nvram_bytes ? chr_ram_bytes : chr_nvram_bytes);
        if (chr_ram_kib == 0) chr_ram_kib = 8;

        pCartridge -> header.ines_header -> prg_ram_size_bytes = prg_ram_kib;
        pCartridge -> header.ines_header -> tv_format = (uint8_t)(pCartridge -> header.raw_header[12] & 0x03);
        pCartridge -> header.ines_header -> padding = 0;

        printf("NES2.0 detected\n"
               "Mapper ID = %u (submapper %u)\n"
               "PRG-ROM = %uKiB\n"
               "CHR-ROM = %uKiB\n"
               "PRG-RAM = %uKiB\n"
               "CHR-RAM = %uKiB\n",
               pCartridge -> header.mapper_id,
               pCartridge -> header.nes2_header -> submapper_id,
               pCartridge -> header.prg_rom_size_bytes,
               pCartridge -> header.chr_rom_size_bytes,
               prg_ram_kib,
               chr_ram_kib);
    } else {
        perror("!Error! Unknown cartrige type (flags7=0x%02X, format_bits=%u)", pCartridge -> header.flags7, format_bits);
        exit(EXIT_FAILURE);
    }

    printf("Allocating PRG-ROM...\n");
    size_t prg_rom_bytes_to_read = KIB(pCartridge -> header.prg_rom_size_bytes);
    pCartridge -> prg_rom = (uint8_t*)malloc(prg_rom_bytes_to_read);
    if(pCartridge -> prg_rom == NULL){
        perror("!Error! Malloc failed for PRG-ROM");
        exit(EXIT_FAILURE);
    }
    printf("Allocated %uKiB for PRG-ROM\n", pCartridge -> header.prg_rom_size_bytes);

    printf("Allocating PRG-RAM...\n");
    if (prg_ram_kib > 0) {
        pCartridge -> prg_ram = (uint8_t*)malloc(KIB(prg_ram_kib));
        if(pCartridge -> prg_ram == NULL){
            perror("!Error! Malloc failed for PRG-RAM");
            exit(EXIT_FAILURE);
        }
        memset(pCartridge -> prg_ram, 0, KIB(prg_ram_kib));
    } else {
        pCartridge -> prg_ram = NULL;
    }
    printf("Allocated %uKiB for PRG-RAM\n", prg_ram_kib);

    if(pCartridge -> header.chr_rom_size_bytes == 0){
        pCartridge -> chr_rom = NULL;
        pCartridge -> has_chr_ram = true;
        printf("No need to allocate CHR-ROM\nAllocating CHR-RAM instead...");
        pCartridge -> chr_ram = (uint8_t*)malloc(KIB(chr_ram_kib));
        if(pCartridge -> chr_ram == NULL){
            perror("!Error! Malloc failed for CHR-RAM");
            exit(EXIT_FAILURE);
        }
        printf("Allocated %uKiB for CHR-RAM\n", chr_ram_kib);
    } else {
        pCartridge -> chr_ram = NULL;
        pCartridge -> has_chr_ram = false;
        printf("Allocating CHR-ROM...\n");
        pCartridge -> chr_rom = (uint8_t*)malloc(KIB(pCartridge -> header.chr_rom_size_bytes));
        if(pCartridge -> chr_rom == NULL){
            perror("!Error! Malloc failed for CHR-ROM");
            exit(EXIT_FAILURE);
        }
        printf("Allocated %uKiB for CHR-ROM\n", pCartridge -> header.chr_rom_size_bytes);
    }

    printf("Allocated %u KiB (%u bytes) of PRG-RAM\n", prg_ram_kib, KIB(prg_ram_kib));

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
    uint32_t pgr_rom_end     = pgr_rom_start + (uint32_t)prg_rom_bytes_to_read;

    printf("Offset for PGR-ROM = (%d -> %d)", pgr_rom_start, pgr_rom_end);

    size_t prgrom_read = fread(pCartridge -> prg_rom, 1, prg_rom_bytes_to_read, cartridge_pointer);
    if(prgrom_read != prg_rom_bytes_to_read){
        perror("!Error! Failed reading PGR-ROM");
        exit(EXIT_FAILURE);
    }

    printf("Read and copied %zuKiB from cartridge to PGR-ROM", prgrom_read / 1024);

    size_t chr_rom_bytes_to_read = KIB(pCartridge -> header.chr_rom_size_bytes);
    uint32_t chr_rom_start   = pgr_rom_end;
    uint32_t chr_rom_end     = chr_rom_start + (uint32_t)chr_rom_bytes_to_read;

    printf("Offset for CHR-ROM = (%d -> %d)", chr_rom_start, chr_rom_end);

    if(pCartridge -> header.chr_rom_size_bytes > 0){
        size_t chrrom_read = fread(pCartridge -> chr_rom, 1, chr_rom_bytes_to_read, cartridge_pointer);
        if(chrrom_read != chr_rom_bytes_to_read){
            perror("!Error! Failed reading CHR-ROM");
            exit(EXIT_FAILURE);
        }

        printf("Read and copied %zuKiB from cartridge to CHR-ROM", chrrom_read / 1024);
    }

    rewind(cartridge_pointer);

    fseek(cartridge_pointer, 0L, SEEK_END);
    size_t sz = ftell(cartridge_pointer);

    if((size_t)chr_rom_end > sz){
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
void free_cartridge(cartridge c) {
    if (!c) return;

    if (c->prg_rom) { free(c->prg_rom); c->prg_rom = NULL;}

    if (c->trainer) {
        free(c->trainer);
        c->trainer = NULL;
    }

    if (c->prg_ram) {
        free(c->prg_ram); c->prg_ram = NULL;
    }

    if (c->chr_rom == NULL) {
        if (c->chr_ram) { free(c->chr_ram); c->chr_ram = NULL; }
        // niente CHR-ROM
        c->chr_rom = NULL;
    } else {
        if (c->chr_rom) { free(c->chr_rom); c->chr_rom = NULL; }
        // niente CHR-RAM
        c->chr_ram = NULL;
    }

    if (c -> header.ines_header) {
        free(c -> header.ines_header);
        c -> header.ines_header = NULL;
    }
    if (c -> header.nes2_header) {
        free(c -> header.nes2_header);
        c -> header.nes2_header = NULL;
    }

    free(c);
}

/**
 * Print a log of the cartridge passed by arg
 * @param pCartridge a Cartridge struct
 */
void print_info(cartridge pCartridge){
    uint32_t prg_ram_kib = (pCartridge && pCartridge -> header.ines_header)
                           ? pCartridge -> header.ines_header -> prg_ram_size_bytes
                           : 0;
    uint32_t chr_ram_kib = pCartridge && pCartridge -> chr_ram ? 8 : 0;
    printf("=== Cartridge Info ===\n"
           "Mapper:      %d\n"
           "Mirroring:   %s\n"
           "Trainer:     %s\n"
           "Battery:     %s\n"
           "PRG-ROM:     %d bytes\n"
           "CHR-ROM:     %d bytes\n"
           "PRG-RAM:     %d bytes\n"
           "CHR-RAM:     %d bytes\n"
           "=======================",
           pCartridge -> header.mapper_id,
           get_mirroring(pCartridge -> header.mirroring),
           pCartridge -> header.has_trainer ? "Yes" : "No",
           pCartridge -> header.has_battery ? "Yes" : "No",
           KIB(pCartridge -> header.prg_rom_size_bytes),
           KIB(pCartridge -> header.chr_rom_size_bytes),
           KIB(prg_ram_kib),
           KIB(chr_ram_kib));
}

/**
 * Create a dummy cartridge for testing purpose
 * @param prg_kib Value of KiB to allocate for PRG-ROM
 * @param chr_kib Value of KiB to allocate for CHR-ROM/RAM
 * @param has_prg_ram Indicate if the Cartridge have PRG-RAM
 * @param chr_is_ram Indicate if the CHR is RAM or ROM
 * @return A pointer to the dummy cartridge
 */
cartridge make_dummy(uint8_t prg_kib, uint8_t chr_kib, bool has_prg_ram, bool chr_is_ram){
    cartridge cart = (cartridge)calloc(1, sizeof(struct Cartridge));
    if(cart == NULL){
        perror("Error allocating dummy cartridge");
        exit(EXIT_FAILURE);
    }

    cart -> header.prg_rom_size_bytes = prg_kib;
    cart -> header.chr_rom_size_bytes = chr_is_ram ? 0 : chr_kib;
    cart -> header.ines_header = (struct ines_header*)malloc(sizeof(struct ines_header));
    if(cart -> header.ines_header == NULL){
        perror("Error allocating iNES header in dummy cartridge");
        exit(EXIT_FAILURE);
    }
    cart -> header.nes2_header = NULL;
    cart -> header.ines_header -> prg_ram_size_bytes = has_prg_ram ? 8 : 0;
    cart -> prg_rom = (uint8_t*)malloc(KIB(1024));
    for(int i = 0; i < KIB(prg_kib); i++){
        uint8_t bank = i / KIB(16);
        cart -> prg_rom[i] = (bank == 0) ? (0xA0 + (i & 0x0F)) :
                             (bank == 1) ? (0xB0 + (i & 0x0F)) :
                                           (0xC0 + (i & 0x0F));
    }
    cart -> prg_ram = has_prg_ram ? (uint8_t*)malloc(KIB(8)) : NULL;
    if(has_prg_ram && cart -> prg_ram == NULL){
        perror("Error allocating PRG-RAM in dummy cartridge");
        exit(EXIT_FAILURE);
    }
    if(has_prg_ram) memset(cart -> prg_ram, 0x00, KIB(8));

    if(chr_is_ram){
        cart -> chr_rom = NULL;
        cart -> chr_ram = (uint8_t*)malloc(KIB(chr_kib));
        if(cart -> chr_ram == NULL){
            perror("Error allocating CHR-RAM in dummy cartridge");
            exit(EXIT_FAILURE);
        }
        for(int i = 0; i < KIB(chr_kib); i++){
            cart -> chr_ram[i] = (0xC0 + (i & 0x1F));
        }
    } else {
        cart -> chr_ram = NULL;
        cart -> chr_rom = (uint8_t*)malloc(KIB(chr_kib));
        if(cart -> chr_rom == NULL){
            perror("Error allocating CHR-ROM in dummy cartridge");
            exit(EXIT_FAILURE);
        }
        for(int i = 0; i < KIB(chr_kib); i++){
            cart -> chr_rom[i] = (0xD0 + (i & 0x1F));
        }
    }

    return cart;
}

uint8_t* getROM(cartridge c){
    return c -> prg_rom;
}

uint8_t* getVROM(cartridge c){
    if(c -> has_chr_ram) return c -> chr_ram;
    else return c -> chr_rom;
}

uint16_t getMapper(cartridge c){
    return c -> header.mapper_id;
}
