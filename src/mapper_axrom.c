//
// Mapper 7 (AxROM)
//

#include "headers/mapper.h"

typedef struct MapperAxROM {
    struct Mapper base;
    uint8_t prg_bank;
    enum mirror_type mirroring;
    bool four_screen_fixed;
} MapperAxROM;

static inline uint32_t axrom_prg_bytes(const MapperAxROM* m) {
    return KIB(m -> base.cart -> header.prg_rom_size_bytes);
}

static uint8_t axrom_cpu_read(mapper base, uint16_t addr) {
    MapperAxROM* m = (MapperAxROM*)base;
    if (addr < 0x8000u) return 0x00;

    uint32_t prg_bytes = axrom_prg_bytes(m);
    if (prg_bytes == 0) return 0x00;
    uint32_t bank_count = prg_bytes / 0x8000u;
    if (bank_count == 0) bank_count = 1;

    uint32_t bank = m -> prg_bank % bank_count;
    uint32_t offset = bank * 0x8000u + (uint32_t)(addr - 0x8000u);
    return m -> base.cart -> prg_rom[offset % prg_bytes];
}

static void axrom_cpu_write(mapper base, uint16_t addr, uint8_t v) {
    MapperAxROM* m = (MapperAxROM*)base;
    if (addr < 0x8000u) return;

    m -> prg_bank = v & 0x07u;
    if (!m -> four_screen_fixed) {
        m -> mirroring = (v & 0x10u) ? ONE_SCREEN_HIGHER : ONE_LOWER_SCREEN;
    }
}

static uint8_t axrom_chr_read(mapper base, uint16_t addr) {
    MapperAxROM* m = (MapperAxROM*)base;
    addr &= 0x1FFFu;
    if (m -> base.cart -> chr_rom) {
        uint32_t chr_bytes = KIB(m -> base.cart -> header.chr_rom_size_bytes);
        if (chr_bytes == 0) return 0x00;
        return m -> base.cart -> chr_rom[addr % chr_bytes];
    }
    if (!m -> base.cart -> chr_ram) return 0x00;
    return m -> base.cart -> chr_ram[addr];
}

static void axrom_chr_write(mapper base, uint16_t addr, uint8_t v) {
    MapperAxROM* m = (MapperAxROM*)base;
    if (!m -> base.cart -> chr_ram || m -> base.cart -> chr_rom) return;
    m -> base.cart -> chr_ram[addr & 0x1FFFu] = v;
}

static enum mirror_type axrom_get_mirror_type(mapper base) {
    MapperAxROM* m = (MapperAxROM*)base;
    return m -> four_screen_fixed ? FOUR_SCREEN : m -> mirroring;
}

static void axrom_scanline_irq(mapper base) {
    (void)base;
}

static void axrom_destroy(mapper base) {
    free(base);
}

mapper mapper_axrom_create(cartridge cart) {
    if (!cart) return NULL;
    MapperAxROM* m = (MapperAxROM*)calloc(1, sizeof(MapperAxROM));
    if (!m) return NULL;

    m -> base.cart = cart;
    m -> base.m_type = AxROM;
    m -> base.cpu_read = axrom_cpu_read;
    m -> base.cpu_write = axrom_cpu_write;
    m -> base.chr_read = axrom_chr_read;
    m -> base.chr_write = axrom_chr_write;
    m -> base.get_mirror_type = axrom_get_mirror_type;
    m -> base.scanlineIRQ = axrom_scanline_irq;
    m -> base.destroy = axrom_destroy;

    m -> prg_bank = 0;
    m -> four_screen_fixed = cart -> header.mirroring == FOUR_SCREEN;
    m -> mirroring = ONE_LOWER_SCREEN;
    return (mapper)m;
}
