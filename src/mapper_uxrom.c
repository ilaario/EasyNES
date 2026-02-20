//
// Mapper 2 (UxROM)
//

#include "headers/mapper.h"

typedef struct MapperUxROM {
    struct Mapper base;
    uint8_t prg_bank;
    enum mirror_type mirroring;
} MapperUxROM;

static inline uint32_t uxrom_prg_bytes(const MapperUxROM* m) {
    return KIB(m -> base.cart -> header.prg_rom_size_bytes);
}

static uint8_t uxrom_cpu_read(mapper base, uint16_t addr) {
    MapperUxROM* m = (MapperUxROM*)base;
    if (addr < 0x8000u) return 0x00;

    uint32_t prg_bytes = uxrom_prg_bytes(m);
    if (prg_bytes == 0) return 0x00;
    uint32_t bank_count = prg_bytes / 0x4000u;
    if (bank_count == 0) bank_count = 1;

    uint32_t bank = (addr < 0xC000u) ? (m -> prg_bank % bank_count) : (bank_count - 1u);
    uint32_t offset = bank * 0x4000u + (uint32_t)(addr & 0x3FFFu);
    return m -> base.cart -> prg_rom[offset % prg_bytes];
}

static void uxrom_cpu_write(mapper base, uint16_t addr, uint8_t v) {
    MapperUxROM* m = (MapperUxROM*)base;
    if (addr < 0x8000u) return;
    m -> prg_bank = v & 0x0Fu;
}

static uint8_t uxrom_chr_read(mapper base, uint16_t addr) {
    MapperUxROM* m = (MapperUxROM*)base;
    addr &= 0x1FFFu;
    if (m -> base.cart -> chr_rom) {
        uint32_t chr_bytes = KIB(m -> base.cart -> header.chr_rom_size_bytes);
        if (chr_bytes == 0) return 0x00;
        return m -> base.cart -> chr_rom[addr % chr_bytes];
    }
    if (!m -> base.cart -> chr_ram) return 0x00;
    return m -> base.cart -> chr_ram[addr];
}

static void uxrom_chr_write(mapper base, uint16_t addr, uint8_t v) {
    MapperUxROM* m = (MapperUxROM*)base;
    if (!m -> base.cart -> chr_ram || m -> base.cart -> chr_rom) return;
    m -> base.cart -> chr_ram[addr & 0x1FFFu] = v;
}

static enum mirror_type uxrom_get_mirror_type(mapper base) {
    return ((MapperUxROM*)base) -> mirroring;
}

static void uxrom_scanline_irq(mapper base) {
    (void)base;
}

static void uxrom_destroy(mapper base) {
    free(base);
}

mapper mapper_uxrom_create(cartridge cart) {
    if (!cart) return NULL;
    MapperUxROM* m = (MapperUxROM*)calloc(1, sizeof(MapperUxROM));
    if (!m) return NULL;

    m -> base.cart = cart;
    m -> base.m_type = UxROM;
    m -> base.cpu_read = uxrom_cpu_read;
    m -> base.cpu_write = uxrom_cpu_write;
    m -> base.chr_read = uxrom_chr_read;
    m -> base.chr_write = uxrom_chr_write;
    m -> base.get_mirror_type = uxrom_get_mirror_type;
    m -> base.scanlineIRQ = uxrom_scanline_irq;
    m -> base.destroy = uxrom_destroy;
    m -> mirroring = cart -> header.mirroring;
    m -> prg_bank = 0;
    return (mapper)m;
}
