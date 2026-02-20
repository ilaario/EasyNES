//
// Mapper 3 (CNROM)
//

#include "headers/mapper.h"

typedef struct MapperCNROM {
    struct Mapper base;
    bool one_prg_bank;
    uint8_t chr_bank;
    enum mirror_type mirroring;
} MapperCNROM;

static inline uint32_t cnrom_prg_bytes(const MapperCNROM* m) {
    return KIB(m -> base.cart -> header.prg_rom_size_bytes);
}

static uint8_t cnrom_cpu_read(mapper base, uint16_t addr) {
    MapperCNROM* m = (MapperCNROM*)base;
    if (addr < 0x8000u) return 0x00;
    uint32_t prg_bytes = cnrom_prg_bytes(m);
    if (prg_bytes == 0) return 0x00;

    uint32_t offset = m -> one_prg_bank
                      ? ((uint32_t)(addr - 0x8000u) & 0x3FFFu)
                      : (uint32_t)(addr - 0x8000u);
    return m -> base.cart -> prg_rom[offset % prg_bytes];
}

static void cnrom_cpu_write(mapper base, uint16_t addr, uint8_t v) {
    MapperCNROM* m = (MapperCNROM*)base;
    if (addr < 0x8000u) return;
    m -> chr_bank = v;
}

static uint8_t cnrom_chr_read(mapper base, uint16_t addr) {
    MapperCNROM* m = (MapperCNROM*)base;
    addr &= 0x1FFFu;

    if (m -> base.cart -> chr_rom) {
        uint32_t chr_bytes = KIB(m -> base.cart -> header.chr_rom_size_bytes);
        if (chr_bytes == 0) return 0x00;
        uint32_t banks_8k = chr_bytes / 0x2000u;
        if (banks_8k == 0) banks_8k = 1;
        uint32_t bank = m -> chr_bank % banks_8k;
        uint32_t offset = bank * 0x2000u + (uint32_t)addr;
        return m -> base.cart -> chr_rom[offset % chr_bytes];
    }

    if (!m -> base.cart -> chr_ram) return 0x00;
    return m -> base.cart -> chr_ram[addr];
}

static void cnrom_chr_write(mapper base, uint16_t addr, uint8_t v) {
    MapperCNROM* m = (MapperCNROM*)base;
    if (!m -> base.cart -> chr_ram || m -> base.cart -> chr_rom) return;
    m -> base.cart -> chr_ram[addr & 0x1FFFu] = v;
}

static enum mirror_type cnrom_get_mirror_type(mapper base) {
    return ((MapperCNROM*)base) -> mirroring;
}

static void cnrom_scanline_irq(mapper base) {
    (void)base;
}

static void cnrom_destroy(mapper base) {
    free(base);
}

mapper mapper_cnrom_create(cartridge cart) {
    if (!cart) return NULL;
    MapperCNROM* m = (MapperCNROM*)calloc(1, sizeof(MapperCNROM));
    if (!m) return NULL;

    m -> base.cart = cart;
    m -> base.m_type = CNROM;
    m -> base.cpu_read = cnrom_cpu_read;
    m -> base.cpu_write = cnrom_cpu_write;
    m -> base.chr_read = cnrom_chr_read;
    m -> base.chr_write = cnrom_chr_write;
    m -> base.get_mirror_type = cnrom_get_mirror_type;
    m -> base.scanlineIRQ = cnrom_scanline_irq;
    m -> base.destroy = cnrom_destroy;

    m -> one_prg_bank = KIB(cart -> header.prg_rom_size_bytes) <= 0x4000u;
    m -> chr_bank = 0;
    m -> mirroring = cart -> header.mirroring;
    return (mapper)m;
}
