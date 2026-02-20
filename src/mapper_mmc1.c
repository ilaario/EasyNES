//
// Created by Dario Bonfiglio on 10/12/25.
//

#include "headers/mapper.h"

typedef struct MapperMMC1 {
    struct Mapper base;
    uint8_t shift_reg;
    uint8_t control;
    uint8_t chr_bank0;
    uint8_t chr_bank1;
    uint8_t prg_bank;
    enum mirror_type mirroring;
    bool four_screen_fixed;
} MapperMMC1;

static inline uint32_t prg_rom_bytes(const MapperMMC1* m) {
    return KIB(m -> base.cart -> header.prg_rom_size_bytes);
}

static inline uint32_t chr_rom_bytes(const MapperMMC1* m) {
    return KIB(m -> base.cart -> header.chr_rom_size_bytes);
}

static inline uint32_t wrap_bank(uint32_t bank, uint32_t count) {
    return (count == 0) ? 0 : (bank % count);
}

static void mmc1_update_mirroring(MapperMMC1* m) {
    if (m -> four_screen_fixed) {
        m -> mirroring = FOUR_SCREEN;
        return;
    }
    switch (m -> control & 0x03u) {
        case 0: m -> mirroring = ONE_LOWER_SCREEN; break;
        case 1: m -> mirroring = ONE_SCREEN_HIGHER; break;
        case 2: m -> mirroring = MIRROR_VERTICAL; break;
        case 3: m -> mirroring = MIRROR_HORIZONTAL; break;
        default: m -> mirroring = m -> base.cart -> header.mirroring; break;
    }
}

static uint8_t mmc1_cpu_read(mapper base, uint16_t addr) {
    MapperMMC1* m = (MapperMMC1*)base;
    if (addr < 0x8000) return 0x00;

    uint32_t prg_bytes = prg_rom_bytes(m);
    if (prg_bytes == 0) return 0x00;

    uint32_t prg_banks_16k = prg_bytes / 0x4000u;
    if (prg_banks_16k == 0) prg_banks_16k = 1;
    uint32_t prg_banks_32k = prg_bytes / 0x8000u;
    if (prg_banks_32k == 0) prg_banks_32k = 1;

    uint32_t offset = 0;
    uint8_t mode = (m -> control >> 2u) & 0x03u;
    switch (mode) {
        case 0:
        case 1: {
            uint32_t bank = wrap_bank((m -> prg_bank & 0x0Eu) >> 1u, prg_banks_32k);
            offset = bank * 0x8000u + (uint32_t)(addr - 0x8000u);
            break;
        }
        case 2: {
            uint32_t bank = (addr < 0xC000u) ? 0u : wrap_bank(m -> prg_bank, prg_banks_16k);
            offset = bank * 0x4000u + (uint32_t)(addr & 0x3FFFu);
            break;
        }
        case 3:
        default: {
            uint32_t bank = (addr < 0xC000u)
                            ? wrap_bank(m -> prg_bank, prg_banks_16k)
                            : (prg_banks_16k - 1u);
            offset = bank * 0x4000u + (uint32_t)(addr & 0x3FFFu);
            break;
        }
    }

    return m -> base.cart -> prg_rom[offset % prg_bytes];
}

static void mmc1_commit_register(MapperMMC1* m, uint16_t addr, uint8_t value) {
    if (addr <= 0x9FFFu) {
        m -> control = value;
        mmc1_update_mirroring(m);
    } else if (addr <= 0xBFFFu) {
        m -> chr_bank0 = value;
    } else if (addr <= 0xDFFFu) {
        m -> chr_bank1 = value;
    } else {
        m -> prg_bank = value;
    }
}

static void mmc1_cpu_write(mapper base, uint16_t addr, uint8_t value) {
    MapperMMC1* m = (MapperMMC1*)base;
    if (addr < 0x8000u) return;

    if (value & 0x80u) {
        m -> shift_reg = 0x10u;
        m -> control |= 0x0Cu;
        mmc1_update_mirroring(m);
        return;
    }

    bool commit = (m -> shift_reg & 0x01u) != 0;
    m -> shift_reg >>= 1u;
    m -> shift_reg |= (uint8_t)((value & 0x01u) << 4u);

    if (commit) {
        mmc1_commit_register(m, addr, m -> shift_reg);
        m -> shift_reg = 0x10u;
    }
}

static uint8_t mmc1_chr_read(mapper base, uint16_t addr) {
    MapperMMC1* m = (MapperMMC1*)base;
    addr &= 0x1FFFu;

    uint8_t* chr_ram = m -> base.cart -> chr_ram;
    uint8_t* chr_rom = m -> base.cart -> chr_rom;
    uint32_t chr_bytes = chr_rom_bytes(m);
    bool use_chr_ram = chr_bytes == 0 || chr_rom == NULL;

    if (use_chr_ram) {
        if (!chr_ram) return 0x00;
        uint32_t offset = (uint32_t)addr;
        if (m -> control & 0x10u) {
            uint32_t bank = (addr < 0x1000u) ? m -> chr_bank0 : m -> chr_bank1;
            offset = (bank * 0x1000u) + (uint32_t)(addr & 0x0FFFu);
        } else {
            uint32_t bank = (m -> chr_bank0 & 0x1Eu) >> 1u;
            offset = (bank * 0x2000u) + (uint32_t)(addr & 0x1FFFu);
        }
        return chr_ram[offset & 0x1FFFu];
    }

    uint32_t offset = 0;
    if (m -> control & 0x10u) {
        uint32_t banks_4k = chr_bytes / 0x1000u;
        if (banks_4k == 0) banks_4k = 1;
        uint32_t bank = (addr < 0x1000u) ? m -> chr_bank0 : m -> chr_bank1;
        bank = wrap_bank(bank, banks_4k);
        offset = bank * 0x1000u + (uint32_t)(addr & 0x0FFFu);
    } else {
        uint32_t banks_8k = chr_bytes / 0x2000u;
        if (banks_8k == 0) banks_8k = 1;
        uint32_t bank = wrap_bank((m -> chr_bank0 & 0x1Eu) >> 1u, banks_8k);
        offset = bank * 0x2000u + (uint32_t)(addr & 0x1FFFu);
    }

    return chr_rom[offset % chr_bytes];
}

static void mmc1_chr_write(mapper base, uint16_t addr, uint8_t v) {
    MapperMMC1* m = (MapperMMC1*)base;
    if (!m -> base.cart -> chr_ram || m -> base.cart -> chr_rom != NULL) return;
    addr &= 0x1FFFu;

    uint32_t offset = 0;
    if (m -> control & 0x10u) {
        uint32_t bank = (addr < 0x1000u) ? m -> chr_bank0 : m -> chr_bank1;
        offset = bank * 0x1000u + (uint32_t)(addr & 0x0FFFu);
    } else {
        uint32_t bank = (m -> chr_bank0 & 0x1Eu) >> 1u;
        offset = bank * 0x2000u + (uint32_t)(addr & 0x1FFFu);
    }

    m -> base.cart -> chr_ram[offset & 0x1FFFu] = v;
}

static enum mirror_type mmc1_get_mirror_type(mapper base) {
    MapperMMC1* m = (MapperMMC1*)base;
    return m -> mirroring;
}

static void mmc1_scanline_irq(mapper base) {
    (void)base;
}

static void mmc1_reset(mapper base) {
    MapperMMC1* m = (MapperMMC1*)base;
    m -> shift_reg = 0x10u;
    m -> control = 0x0Cu;
    m -> chr_bank0 = 0;
    m -> chr_bank1 = 0;
    m -> prg_bank = 0;
    mmc1_update_mirroring(m);
}

static void mmc1_destroy(mapper base) {
    free(base);
}

mapper mapper_mmc1_create(cartridge cart) {
    if (!cart) return NULL;
    MapperMMC1* m = (MapperMMC1*)calloc(1, sizeof(MapperMMC1));
    if (!m) return NULL;

    m -> base.cart = cart;
    m -> base.m_type = SxROM;
    m -> base.cpu_read = mmc1_cpu_read;
    m -> base.cpu_write = mmc1_cpu_write;
    m -> base.chr_read = mmc1_chr_read;
    m -> base.chr_write = mmc1_chr_write;
    m -> base.get_mirror_type = mmc1_get_mirror_type;
    m -> base.scanlineIRQ = mmc1_scanline_irq;
    m -> base.reset = mmc1_reset;
    m -> base.destroy = mmc1_destroy;
    m -> four_screen_fixed = cart -> header.mirroring == FOUR_SCREEN;

    mmc1_reset((mapper)m);
    return (mapper)m;
}
