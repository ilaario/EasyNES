//
// Mapper 4 (MMC3) - functional implementation for common games.
//

#include "headers/mapper.h"
#include <string.h>

typedef struct MapperMMC3 {
    struct Mapper base;
    irq_handle irq;

    uint8_t bank_select;
    uint8_t bank_regs[8];

    uint8_t irq_latch;
    uint8_t irq_counter;
    bool irq_reload;
    bool irq_enabled;

    enum mirror_type mirroring;
    bool four_screen_fixed;
} MapperMMC3;

static inline uint32_t mmc3_prg_bytes(const MapperMMC3* m) {
    return KIB(m -> base.cart -> header.prg_rom_size_bytes);
}

static inline uint32_t mmc3_chr_bytes(const MapperMMC3* m) {
    return KIB(m -> base.cart -> header.chr_rom_size_bytes);
}

static inline uint32_t mmc3_wrap(uint32_t value, uint32_t count) {
    return (count == 0) ? 0 : (value % count);
}

static uint8_t mmc3_cpu_read(mapper base, uint16_t addr) {
    MapperMMC3* m = (MapperMMC3*)base;
    if (addr < 0x8000u) return 0x00;

    uint32_t prg_bytes = mmc3_prg_bytes(m);
    if (prg_bytes == 0) return 0x00;

    uint32_t banks_8k = prg_bytes / 0x2000u;
    if (banks_8k == 0) banks_8k = 1;
    uint32_t last = banks_8k - 1u;
    uint32_t last2 = (banks_8k >= 2u) ? (banks_8k - 2u) : 0u;

    bool prg_mode = (m -> bank_select & 0x40u) != 0;
    uint32_t slot0 = prg_mode ? last2 : mmc3_wrap(m -> bank_regs[6], banks_8k);
    uint32_t slot1 = mmc3_wrap(m -> bank_regs[7], banks_8k);
    uint32_t slot2 = prg_mode ? mmc3_wrap(m -> bank_regs[6], banks_8k) : last2;
    uint32_t slot3 = last;

    uint32_t slot_bank;
    if (addr < 0xA000u) slot_bank = slot0;
    else if (addr < 0xC000u) slot_bank = slot1;
    else if (addr < 0xE000u) slot_bank = slot2;
    else slot_bank = slot3;

    uint32_t offset = slot_bank * 0x2000u + (uint32_t)(addr & 0x1FFFu);
    return m -> base.cart -> prg_rom[offset % prg_bytes];
}

static void mmc3_cpu_write(mapper base, uint16_t addr, uint8_t v) {
    MapperMMC3* m = (MapperMMC3*)base;
    if (addr < 0x8000u) return;

    switch (addr & 0xE001u) {
        case 0x8000u:
            m -> bank_select = v;
            break;
        case 0x8001u:
            m -> bank_regs[m -> bank_select & 0x07u] = v;
            break;
        case 0xA000u:
            if (!m -> four_screen_fixed) {
                m -> mirroring = (v & 0x01u) ? MIRROR_HORIZONTAL : MIRROR_VERTICAL;
            }
            break;
        case 0xA001u:
            // PRG RAM protect/enable not modeled yet (RAM stays always visible on CPU bus).
            break;
        case 0xC000u:
            m -> irq_latch = v;
            break;
        case 0xC001u:
            m -> irq_reload = true;
            break;
        case 0xE000u:
            m -> irq_enabled = false;
            if (m -> irq) m -> irq -> release(m -> irq);
            break;
        case 0xE001u:
            m -> irq_enabled = true;
            break;
        default:
            break;
    }
}

static uint32_t mmc3_chr_bank_for_addr(const MapperMMC3* m, uint16_t addr) {
    bool chr_mode = (m -> bank_select & 0x80u) != 0;
    addr &= 0x1FFFu;

    if (!chr_mode) {
        if (addr < 0x0400u) return (m -> bank_regs[0] & 0xFEu);
        if (addr < 0x0800u) return (m -> bank_regs[0] & 0xFEu) + 1u;
        if (addr < 0x0C00u) return (m -> bank_regs[1] & 0xFEu);
        if (addr < 0x1000u) return (m -> bank_regs[1] & 0xFEu) + 1u;
        if (addr < 0x1400u) return m -> bank_regs[2];
        if (addr < 0x1800u) return m -> bank_regs[3];
        if (addr < 0x1C00u) return m -> bank_regs[4];
        return m -> bank_regs[5];
    }

    if (addr < 0x0400u) return m -> bank_regs[2];
    if (addr < 0x0800u) return m -> bank_regs[3];
    if (addr < 0x0C00u) return m -> bank_regs[4];
    if (addr < 0x1000u) return m -> bank_regs[5];
    if (addr < 0x1400u) return (m -> bank_regs[0] & 0xFEu);
    if (addr < 0x1800u) return (m -> bank_regs[0] & 0xFEu) + 1u;
    if (addr < 0x1C00u) return (m -> bank_regs[1] & 0xFEu);
    return (m -> bank_regs[1] & 0xFEu) + 1u;
}

static uint8_t mmc3_chr_read(mapper base, uint16_t addr) {
    MapperMMC3* m = (MapperMMC3*)base;
    addr &= 0x1FFFu;

    uint32_t bank = mmc3_chr_bank_for_addr(m, addr);
    uint32_t index = (uint32_t)(addr & 0x03FFu);

    if (m -> base.cart -> chr_rom) {
        uint32_t chr_bytes = mmc3_chr_bytes(m);
        if (chr_bytes == 0) return 0x00;
        uint32_t banks_1k = chr_bytes / 0x0400u;
        if (banks_1k == 0) banks_1k = 1;
        uint32_t offset = mmc3_wrap(bank, banks_1k) * 0x0400u + index;
        return m -> base.cart -> chr_rom[offset % chr_bytes];
    }

    if (!m -> base.cart -> chr_ram) return 0x00;
    uint32_t offset = (bank * 0x0400u + index) & 0x1FFFu;
    return m -> base.cart -> chr_ram[offset];
}

static void mmc3_chr_write(mapper base, uint16_t addr, uint8_t v) {
    MapperMMC3* m = (MapperMMC3*)base;
    if (!m -> base.cart -> chr_ram || m -> base.cart -> chr_rom) return;
    addr &= 0x1FFFu;

    uint32_t bank = mmc3_chr_bank_for_addr(m, addr);
    uint32_t offset = (bank * 0x0400u + (uint32_t)(addr & 0x03FFu)) & 0x1FFFu;
    m -> base.cart -> chr_ram[offset] = v;
}

static enum mirror_type mmc3_get_mirror_type(mapper base) {
    MapperMMC3* m = (MapperMMC3*)base;
    return m -> four_screen_fixed ? FOUR_SCREEN : m -> mirroring;
}

static void mmc3_scanline_irq(mapper base) {
    MapperMMC3* m = (MapperMMC3*)base;
    if (m -> irq_counter == 0 || m -> irq_reload) {
        m -> irq_counter = m -> irq_latch;
        m -> irq_reload = false;
    } else {
        m -> irq_counter -= 1;
    }

    if (m -> irq_counter == 0 && m -> irq_enabled && m -> irq) {
        m -> irq -> pull(m -> irq);
    }
}

static void mmc3_destroy(mapper base) {
    free(base);
}

mapper mapper_mmc3_create(cartridge cart, irq_handle irq) {
    if (!cart) return NULL;
    MapperMMC3* m = (MapperMMC3*)calloc(1, sizeof(MapperMMC3));
    if (!m) return NULL;

    m -> base.cart = cart;
    m -> base.m_type = MMC3;
    m -> base.cpu_read = mmc3_cpu_read;
    m -> base.cpu_write = mmc3_cpu_write;
    m -> base.chr_read = mmc3_chr_read;
    m -> base.chr_write = mmc3_chr_write;
    m -> base.get_mirror_type = mmc3_get_mirror_type;
    m -> base.scanlineIRQ = mmc3_scanline_irq;
    m -> base.destroy = mmc3_destroy;

    m -> irq = irq;
    m -> bank_select = 0;
    memset(m -> bank_regs, 0, sizeof(m -> bank_regs));
    m -> irq_latch = 0;
    m -> irq_counter = 0;
    m -> irq_reload = false;
    m -> irq_enabled = false;
    m -> four_screen_fixed = cart -> header.mirroring == FOUR_SCREEN;
    m -> mirroring = cart -> header.mirroring;
    return (mapper)m;
}
