//
// Mapper 11 (Color Dreams)
//

#include "headers/mapper.h"

typedef struct MapperColorDreams {
    struct Mapper base;
    uint8_t prg_bank;
    uint8_t chr_bank;
    enum mirror_type mirroring;
} MapperColorDreams;

static uint8_t cd_cpu_read(mapper base, uint16_t addr) {
    MapperColorDreams* m = (MapperColorDreams*)base;
    if (addr < 0x8000u) return 0x00;

    uint32_t prg_bytes = KIB(m -> base.cart -> header.prg_rom_size_bytes);
    if (prg_bytes == 0) return 0x00;
    uint32_t bank_count = prg_bytes / 0x8000u;
    if (bank_count == 0) bank_count = 1;

    uint32_t bank = m -> prg_bank % bank_count;
    uint32_t offset = bank * 0x8000u + (uint32_t)(addr - 0x8000u);
    return m -> base.cart -> prg_rom[offset % prg_bytes];
}

static void cd_cpu_write(mapper base, uint16_t addr, uint8_t v) {
    MapperColorDreams* m = (MapperColorDreams*)base;
    if (addr < 0x8000u) return;
    m -> prg_bank = v & 0x03u;
    m -> chr_bank = (v >> 4u) & 0x0Fu;
}

static uint8_t cd_chr_read(mapper base, uint16_t addr) {
    MapperColorDreams* m = (MapperColorDreams*)base;
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

static void cd_chr_write(mapper base, uint16_t addr, uint8_t v) {
    MapperColorDreams* m = (MapperColorDreams*)base;
    if (!m -> base.cart -> chr_ram || m -> base.cart -> chr_rom) return;
    m -> base.cart -> chr_ram[addr & 0x1FFFu] = v;
}

static enum mirror_type cd_get_mirror_type(mapper base) {
    return ((MapperColorDreams*)base) -> mirroring;
}

static void cd_scanline_irq(mapper base) {
    (void)base;
}

static void cd_destroy(mapper base) {
    free(base);
}

mapper mapper_colordreams_create(cartridge cart) {
    if (!cart) return NULL;
    MapperColorDreams* m = (MapperColorDreams*)calloc(1, sizeof(MapperColorDreams));
    if (!m) return NULL;

    m -> base.cart = cart;
    m -> base.m_type = ColorDreams;
    m -> base.cpu_read = cd_cpu_read;
    m -> base.cpu_write = cd_cpu_write;
    m -> base.chr_read = cd_chr_read;
    m -> base.chr_write = cd_chr_write;
    m -> base.get_mirror_type = cd_get_mirror_type;
    m -> base.scanlineIRQ = cd_scanline_irq;
    m -> base.destroy = cd_destroy;

    m -> prg_bank = 0;
    m -> chr_bank = 0;
    m -> mirroring = cart -> header.mirroring;
    return (mapper)m;
}
