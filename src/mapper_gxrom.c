//
// Mapper 66 (GxROM)
//

#include "headers/mapper.h"

typedef struct MapperGxROM {
    struct Mapper base;
    uint8_t prg_bank;
    uint8_t chr_bank;
    enum mirror_type mirroring;
} MapperGxROM;

static uint8_t gxrom_cpu_read(mapper base, uint16_t addr) {
    MapperGxROM* m = (MapperGxROM*)base;
    if (addr < 0x8000u) return 0x00;

    uint32_t prg_bytes = KIB(m -> base.cart -> header.prg_rom_size_bytes);
    if (prg_bytes == 0) return 0x00;
    uint32_t bank_count = prg_bytes / 0x8000u;
    if (bank_count == 0) bank_count = 1;

    uint32_t bank = m -> prg_bank % bank_count;
    uint32_t offset = bank * 0x8000u + (uint32_t)(addr - 0x8000u);
    return m -> base.cart -> prg_rom[offset % prg_bytes];
}

static void gxrom_cpu_write(mapper base, uint16_t addr, uint8_t v) {
    MapperGxROM* m = (MapperGxROM*)base;
    if (addr < 0x8000u) return;
    m -> prg_bank = (v >> 4u) & 0x03u;
    m -> chr_bank = v & 0x03u;
}

static uint8_t gxrom_chr_read(mapper base, uint16_t addr) {
    MapperGxROM* m = (MapperGxROM*)base;
    addr &= 0x1FFFu;

    if (m -> base.cart -> chr_rom) {
        uint32_t chr_bytes = KIB(m -> base.cart -> header.chr_rom_size_bytes);
        if (chr_bytes == 0) return 0x00;
        uint32_t bank_count = chr_bytes / 0x2000u;
        if (bank_count == 0) bank_count = 1;
        uint32_t bank = m -> chr_bank % bank_count;
        uint32_t offset = bank * 0x2000u + (uint32_t)addr;
        return m -> base.cart -> chr_rom[offset % chr_bytes];
    }

    if (!m -> base.cart -> chr_ram) return 0x00;
    return m -> base.cart -> chr_ram[addr];
}

static void gxrom_chr_write(mapper base, uint16_t addr, uint8_t v) {
    MapperGxROM* m = (MapperGxROM*)base;
    if (!m -> base.cart -> chr_ram || m -> base.cart -> chr_rom) return;
    m -> base.cart -> chr_ram[addr & 0x1FFFu] = v;
}

static enum mirror_type gxrom_get_mirror_type(mapper base) {
    return ((MapperGxROM*)base) -> mirroring;
}

static void gxrom_scanline_irq(mapper base) {
    (void)base;
}

static void gxrom_destroy(mapper base) {
    free(base);
}

mapper mapper_gxrom_create(cartridge cart) {
    if (!cart) return NULL;
    MapperGxROM* m = (MapperGxROM*)calloc(1, sizeof(MapperGxROM));
    if (!m) return NULL;

    m -> base.cart = cart;
    m -> base.m_type = GxROM;
    m -> base.cpu_read = gxrom_cpu_read;
    m -> base.cpu_write = gxrom_cpu_write;
    m -> base.chr_read = gxrom_chr_read;
    m -> base.chr_write = gxrom_chr_write;
    m -> base.get_mirror_type = gxrom_get_mirror_type;
    m -> base.scanlineIRQ = gxrom_scanline_irq;
    m -> base.destroy = gxrom_destroy;

    m -> prg_bank = 0;
    m -> chr_bank = 0;
    m -> mirroring = cart -> header.mirroring;
    return (mapper)m;
}
