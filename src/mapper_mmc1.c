//
// Created by Dario Bonfiglio on 10/12/25.
//

#include "headers/mapper.h"
#include <stdlib.h>
#include <string.h>

struct MMC1 {
    struct Mapper base;

    // cart pointers
    cartridge cart;

    // PRG
    const uint8_t *prg;   // PRG-ROM
    size_t prg_size;      // bytes
    int prg_bank_count;   // # of 16 KiB banks

    // CHR (ROM o RAM)
    uint8_t *chr;         // se CHR-RAM, buffer allocato; se CHR-ROM, puntatore ROM (non libero)
    size_t chr_size;
    int chr_is_ram;

    // MMC1 regs
    uint8_t shift;        // 5-bit shift reg (bit0 è l’ultimo immesso)
    int     shift_count;  // 0..5
    uint8_t control;      // $8000-$9FFF
    uint8_t chr_bank0;    // $A000-$BFFF
    uint8_t chr_bank1;    // $C000-$DFFF
    uint8_t prg_bank;     // $E000-$FFFF

    // banking cache
    int prg_mode;         // (control >> 2) & 3
    int chr_mode;         // (control >> 4) & 1
};

typedef struct MMC1* mapperMMC1;

static uint8_t mmc1_cpu_read(mapper m, uint16_t addr) {
    mapperMMC1 mm = (mapperMMC1)m;
    if (addr < 0x8000) return 0xFF;

    // PRG banking (16 KiB pages)
    int bank16 = 0;
    if (mm->prg_mode == 0 || mm->prg_mode == 1) {
        // 32 KiB mode (ignore low bit)
        bank16 = (mm->prg_bank & ~1);
        if (addr < 0xC000) {
            // lower 16K of the 32K pair
            size_t off = (size_t)bank16 * 0x4000 + (addr - 0x8000);
            return mm->prg[off % mm->prg_size];
        } else {
            // upper 16K
            size_t off = (size_t)(bank16 + 1) * 0x4000 + (addr - 0xC000);
            return mm->prg[off % mm->prg_size];
        }
    } else if (mm->prg_mode == 2) {
        // Fix $8000 = bank 0, switch $C000 = prg_bank
        if (addr < 0xC000) {
            size_t off = 0 + (addr - 0x8000);
            return mm->prg[off % mm->prg_size];
        } else {
            int b = (mm->prg_bank % mm->prg_bank_count);
            size_t off = (size_t)b * 0x4000 + (addr - 0xC000);
            return mm->prg[off % mm->prg_size];
        }
    } else {
        // prg_mode == 3: Fix $C000 = last bank, switch $8000 = prg_bank
        if (addr < 0xC000) {
            int b = (mm->prg_bank % mm->prg_bank_count);
            size_t off = (size_t)b * 0x4000 + (addr - 0x8000);
            return mm->prg[off % mm->prg_size];
        } else {
            int last = mm->prg_bank_count - 1;
            size_t off = (size_t)last * 0x4000 + (addr - 0xC000);
            return mm->prg[off % mm->prg_size];
        }
    }
}

static void mmc1_write_control(mapperMMC1 mm, uint8_t v) {
    mm->control  = v | 0x0C; // assicura bit2-3 = 1 su power-on/quando resetti shift (comportamento hw)
    mm->prg_mode = (mm->control >> 2) & 3;
    mm->chr_mode = (mm->control >> 4) & 1;
    // Mirroring (control&3): 0=one-screen lo,1=one-screen hi,2=vertical,3=horizontal
    // Per ora: ignora e lascia quello dell’header (PPU già inizializzato).
}

static void mmc1_cpu_write(mapper m, uint16_t addr, uint8_t value) {
    mapperMMC1 mm = (mapperMMC1)m;
    if (addr < 0x8000) return;

    if (value & 0x80) {
        // Reset shift reg
        mm->shift = 0;
        mm->shift_count = 0;
        mmc1_write_control(mm, (uint8_t)(mm->control | 0x0C)); // forced
        return;
    }

    // Shift in (LSB first)
    mm->shift = (uint8_t)((mm->shift >> 1) | ((value & 1) << 4));
    mm->shift_count++;

    if (mm->shift_count == 5) {
        uint8_t data = mm->shift;
        mm->shift = 0;
        mm->shift_count = 0;

        if (addr < 0xA000) {
            mmc1_write_control(mm, data);
        } else if (addr < 0xC000) {
            mm->chr_bank0 = data;
        } else if (addr < 0xE000) {
            mm->chr_bank1 = data;
        } else {
            mm->prg_bank = data & 0x0F;
        }
    }
}

static uint8_t mmc1_chr_read(mapper m, uint16_t addr) {
    mapperMMC1 mm = (mapperMMC1)m;
    // chr_mode 0 = 8KiB at chr_bank0&~1, mode 1 = 4KiB split
    if (mm->chr_is_ram) {
        if (mm->chr_mode == 0) {
            int bank8 = (mm->chr_bank0 & ~1);
            size_t base = (size_t)bank8 * 0x2000;
            return mm->chr[(base + addr) % mm->chr_size];
        } else {
            if (addr < 0x1000) {
                int b = mm->chr_bank0;
                size_t base = (size_t)b * 0x1000;
                return mm->chr[(base + addr) % mm->chr_size];
            } else {
                int b = mm->chr_bank1;
                size_t base = (size_t)b * 0x1000;
                return mm->chr[(base + (addr - 0x1000)) % mm->chr_size];
            }
        }
    } else {
        // ROM: stessa logica, ma su ROM
        const uint8_t *rom = mm->cart->chr_rom;
        size_t romsz = KIB(mm->cart->header.chr_rom_size_bytes);
        if (mm->chr_mode == 0) {
            int bank8 = (mm->chr_bank0 & ~1);
            size_t base = (size_t)bank8 * 0x2000;
            return rom[(base + addr) % romsz];
        } else {
            if (addr < 0x1000) {
                int b = mm->chr_bank0;
                size_t base = (size_t)b * 0x1000;
                return rom[(base + addr) % romsz];
            } else {
                int b = mm->chr_bank1;
                size_t base = (size_t)b * 0x1000;
                return rom[(base + (addr - 0x1000)) % romsz];
            }
        }
    }
}

static void mmc1_chr_write(mapper m, uint16_t addr, uint8_t v) {
    mapperMMC1 mm = (mapperMMC1)m;
    if (!mm->chr_is_ram) return; // CHR-ROM non scrivibile
    if (mm->chr_mode == 0) {
        int bank8 = (mm->chr_bank0 & ~1);
        size_t base = (size_t)bank8 * 0x2000;
        mm->chr[(base + addr) % mm->chr_size] = v;
    } else {
        if (addr < 0x1000) {
            int b = mm->chr_bank0;
            size_t base = (size_t)b * 0x1000;
            mm->chr[(base + addr) % mm->chr_size] = v;
        } else {
            int b = mm->chr_bank1;
            size_t base = (size_t)b * 0x1000;
            mm->chr[(base + (addr - 0x1000)) % mm->chr_size] = v;
        }
    }
}

static void mmc1_destroy(mapper m) {
    mapperMMC1 mm = (mapperMMC1)m;
    if (mm->chr_is_ram && mm->chr) free(mm->chr);
    free(mm);
}

mapper mapper_mmc1_create(cartridge cart) {
    mapperMMC1 mm = (mapperMMC1)calloc(1, sizeof(struct MMC1));
    if (!mm) return NULL;

    mm->base.cpu_read  = mmc1_cpu_read;
    mm->base.cpu_write = mmc1_cpu_write;
    mm->base.chr_read  = mmc1_chr_read;
    mm->base.chr_write = mmc1_chr_write;
    mm->base.reset   = mmc1_destroy;

    mm->cart = cart;
    mm->prg  = cart->prg_rom;
    mm->prg_size = KIB(cart->header.prg_rom_size_bytes);
    mm->prg_bank_count = (int)(mm->prg_size / 0x4000);

    // CHR: se size==0 -> CHR-RAM 8KiB
    if (KIB(cart->header.chr_rom_size_bytes) == 0) {
        mm->chr_is_ram = 1;
        mm->chr_size   = 0x2000;
        mm->chr        = (uint8_t*)calloc(1, mm->chr_size);
        if (!mm->chr) { free(mm); return NULL; }
    } else {
        mm->chr_is_ram = 0;
        mm->chr_size   = KIB(cart->header.chr_rom_size_bytes);
        mm->chr        = NULL;
    }

    // Power-on state
    mm->shift = 0; mm->shift_count = 0;
    mm->control = 0x0C;      // PRG mode=3, CHR=8KB
    mm->prg_bank = 0;
    mm->chr_bank0 = 0;
    mm->chr_bank1 = 0;
    mm->prg_mode = 3;
    mm->chr_mode = 0;

    return (mapper)mm;
}