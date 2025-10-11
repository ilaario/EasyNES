#include "headers/main.h"

cartridge cart;

int main(int argc, char const *argv[]) {
    log_init("log/easynes.log");

    if (argc != 2) {
        fprintf(stderr, "Usage: <game>.nes\n");
        return EXIT_FAILURE;
    }

    // cart = read_allocate_cartridge(argv[1]);
    cart = make_dummy(16, 8, false, false);

    print_info(cart);

    printf("Cartridge read - DONE");

    printf("Init...");
    bus bus1 = (bus)malloc(sizeof(struct CPUBus));
    ppu ppu1 = (ppu)malloc(sizeof(struct PPU));
    mapper mapper1 = mapper_nrom_create(cart);
    controller pad1 = NULL;
    controller pad2 = NULL;

    ppu_init(ppu1, mapper1, 0);
    bus_init(bus1, ppu1, mapper1, pad1, pad2);

    printf("Init ended");

    printf(" ======================= RAM TEST ======================= ");
    printf("Writing 0xAA at position 0x0000 to check mirroring RAM... ");

    bus_cpu_write8(bus1, 0x0000, 0xAA);

    printf("Ram at pos 0x0000 = 0x%02X", bus_cpu_read8(bus1, 0x0000));
    printf("Ram at pos 0x0800 = 0x%02X", bus_cpu_read8(bus1, 0x0800));
    printf("Ram at pos 0x1000 = 0x%02X", bus_cpu_read8(bus1, 0x1000));
    printf("Ram at pos 0x1800 = 0x%02X", bus_cpu_read8(bus1, 0x1800));

    printf(" ======================= PPU TEST ======================= ");
    printf("Writing 0x11 at position 0x2000 to check mirroring PPU... ");

    // Scrivo in 0x2000 (cioè registro 0)
    bus_cpu_write8(bus1, 0x2000, 0x11);

    printf("PPU at reg 0x2008 = 0x%02X", bus_cpu_read8(bus1, 0x2008));
    printf("PPU at reg 0x3FF8 = 0x%02X", bus_cpu_read8(bus1, 0x3FF8));

    printf("Setting VBlank = 1...");
    ppu1 -> ppustatus |= 0x80;
    printf("VBlank set to 1");
    printf("Side-effect on $2002 = %s (Should be true)", (bus_cpu_read8(bus1, 0x2002) & 0x80) != 0 ? "True" : "False");
    printf("Flag cleared = %s (Should be true)", (ppu1 -> ppustatus & 0x80) == 0 ? "True" : "False");

    printf("Writing on CIRAM...");
    bus_cpu_write8(bus1, 0x2006, 0x20);          // high
    bus_cpu_write8(bus1, 0x2006, 0x00);          // low → v=0x2000
    bus_cpu_write8(bus1, 0x2007, 0x12);          // CIRAM[0]=0x12
    printf("Written 0x12 in $2007 after moving address");

    printf("Setting address to the same point as before...");
    bus_cpu_write8(bus1, 0x2006, 0x20);          // high
    bus_cpu_write8(bus1, 0x2006, 0x00);          // low → v=0x2000
    bus_cpu_read8(bus1, 0x2007);
    printf("First read ignored, second read = 0x%02X", bus_cpu_read8(bus1, 0x2007));

    printf("Testing VBlank without NMI...");
    ppu_reset(ppu1);
    bus_cpu_write8(bus1, 0x2000, 0x00);
    DEBUG_goto_scanline_dot(ppu1, 241, 1);
    printf("VBlank = %d (Should be 1 at (241,1) without NMI)", (ppu1 -> ppustatus & 0x80) != 0);
    printf("NMI = %s (NMI should not be pending if NMI is off)", ppu1 -> nmi_pending ? "True" : "False");

    printf("Testing VBlank with NMI...");
    ppu_reset(ppu1);
    bus_cpu_write8(bus1, 0x2000, 0x80);
    DEBUG_goto_scanline_dot(ppu1, 241, 1);
    printf("VBlank = %s (Should be true at (241,1) with NMI)", (ppu1 -> ppustatus & 0x80) != 0 ? "True" : "False");
    printf("NMI = %s (NMI should be pending if NMI is on)", ppu1 -> nmi_pending ? "True" : "False");
    ppu_clear_nmi(ppu1);
    printf("NMI = %s (NMI should not be pending after clear)", ppu1 -> nmi_pending ? "True" : "False");

    printf("Testing Pre-Render...");
    ppu_reset(ppu1);
    bus_cpu_write8(bus1, 0x2000, 0x80);
    DEBUG_goto_scanline_dot(ppu1, 241, 1);
    printf("Simulated to (241, 1), now in VBlank");
    printf("VBlank = %s", (ppu1 -> ppustatus & 0x80) != 0 ? "True" : "False");
    printf("Starting Pre-Render...");
    DEBUG_goto_scanline_dot(ppu1, 260, 1);
    printf("VBlank = %s (VBlank should be false at pre-render)", (ppu1 -> ppustatus & 0x80) != 0 ? "True" : "False");

    printf("Testing Side Effect...");
    ppu_reset(ppu1);
    bus_cpu_write8(bus1, 0x2000, 0x00);
    DEBUG_goto_scanline_dot(ppu1, 241, 1);
    printf("Read from $2002 = 0x%02X", (bus_cpu_read8(bus1, 0x2002) & 0x80));
    printf("VBlank = %s (VBlank should be false after read in $2002)", (ppu1 -> ppustatus & 0x80) != 0 ? "True" : "False");
    printf("Write toggle = %s (Should be false after read in $2002)", ppu1 -> w != 0 ? "True" : "False");

    printf("Testing increment of Coarse X...");
    ppu_reset(ppu1);
    bus_cpu_write8(bus1, 0x2001, 0x08);
    ppu1 -> v = 0x0005;
    DEBUG_goto_scanline_dot(ppu1, 0, 8);
    printf("PPU -> V = 0x%04X (Should be 0x0006)", ppu1 -> v);
    ppu_reset(ppu1);
    bus_cpu_write8(bus1, 0x2001, 0x08);
    ppu1 -> v = 0x001F;
    DEBUG_goto_scanline_dot(ppu1, 0, 8);      // primo boundary (wrap → 0x0400)
    printf("PPU -> V = 0x%04X (Should be 0x0400)", ppu1 -> v);
    DEBUG_goto_scanline_dot(ppu1, 0, 16);     // secondo boundary (→ 0x0401)
    printf("PPU -> V = 0x%04X (Should be 0x0401)", ppu1 -> v);

    printf("Testing vertical increment...");

// ===== Caso 1: fineY < 7 ⇒ fineY++ =====
    ppu_reset(ppu1);
    bus_cpu_write8(bus1, 0x2001, 0x00);               // rendering OFF (niente inc X)
    ppu1->v = (0x3000) | (10 << 5) | 0x00;                          // fineY=3, coarseY=10, coarseX=0, NTX/NTY=0
    DEBUG_goto_scanline_dot(ppu1, 0, 255);     // fermati PRIMA del 256

    bus_cpu_write8(bus1, 0x2001, 0x08);            // rendering ON solo per il ciclo 256
    step_one_ppu_cycle(ppu1);                               // esegue dot=256 ⇒ SOLO increment_y
    printf("PPU -> V = 0x%04X (Should be 0x%04X)\n",
           ppu1->v, (0x4000 | (10 << 5)));                       // 0x4140 atteso

// ===== Caso 2: fineY = 7 ⇒ carry su coarseY =====
    ppu_reset(ppu1);
    bus_cpu_write8(bus1, 0x2001, 0x00);            // rendering OFF
    ppu1->v = (0x7000) | (12 << 5) | 0x00;                       // fineY=7, coarseY=12, coarseX=0
    DEBUG_goto_scanline_dot(ppu1, 0, 255);

    bus_cpu_write8(bus1, 0x2001, 0x08);             // ON solo per il 256
    step_one_ppu_cycle(ppu1);                                // dot=256 ⇒ fineY=0, coarseY=13
    printf("PPU -> V = 0x%04X (Should be 0x%04X)",
           ppu1->v, (0x0000 | (13 << 5)));

// ===== Caso 3: coarseY = 29 ⇒ 0 e toggle NTY =====
    ppu_reset(ppu1);
    bus_cpu_write8(bus1, 0x2001, 0x08);
    ppu1 -> v = 0x7000 | (29 << 5);
    DEBUG_goto_scanline_dot(ppu1, 0, 256);
    printf("PPU -> V = 0x%04X (Should be 0x0000)\n",
           (ppu1->v & 0x03E0));
    printf("NTY = %s (Should be false)\n",
           (ppu1->v & 0x0800) == 0 ? "True" : "False");

// ===== Caso 4: coarseY = 31 ⇒ 0 senza toggle NTY =====
    ppu_reset(ppu1);
    bus_cpu_write8(bus1, 0x2001, 0x08);
    ppu1 -> v = 0x7000 | (31 << 5);
    DEBUG_goto_scanline_dot(ppu1, 0, 256);
    printf("PPU -> V = 0x%04X (Should be 0x0000)\n",
           (ppu1->v & 0x03E0));
    printf("NTY = %s (Should be true)\n",
           (ppu1->v & 0x0800) == 0 ? "True" : "False");

    printf("Vertical copy on pre-render...");
    ppu_reset(ppu1);
    bus_cpu_write8(bus1, 0x2001, 0x08);
    ppu1 -> t = 0x7000 | 0x0800 | (17 << 5);
    DEBUG_goto_scanline_dot(ppu1, 261, 304);
    printf("PPU -> V == PPU -> T is %s (Should be true)",
           (ppu1 -> v & 0x7BE0) == (ppu1 -> t & 0x7BE0) ? "True" : "False");

    printf("Testing full scroll sequence on the same line...");
    ppu_reset(ppu1);
    bus_cpu_write8(bus1, 0x2001, 0x00);
    DEBUG_goto_scanline_dot(ppu1, 5, 7);
    ppu1->v = 0x001F;
    bus_cpu_write8(bus1, 0x2001, 0x08);
    step_one_ppu_cycle(ppu1);
    printf("PPU -> V = 0x%04X (Should be 0x0400)\n", ppu1->v);

    printf(" ====================== OAMDMA TEST ====================== ");
    printf("Testing OAMADDR and OAMDATA sequence...");
    ppu_reset(ppu1);

    bus_cpu_write8(bus1, 0x2003, 0xFC);      // OAMADDR=0xFC
    bus_cpu_write8(bus1, 0x2004, 0x11);
    bus_cpu_write8(bus1, 0x2004, 0x22);
    bus_cpu_write8(bus1, 0x2004, 0x33);
    bus_cpu_write8(bus1, 0x2004, 0x44);

    printf("PPU -> OAMADDR   = 0x%02X (Should be 0x00)", ppu1 -> oamaddr);
    printf("PPU -> OAM[0xFC] = 0x%02X (Should be 0x11)", ppu1 -> oam[0xFC]);
    printf("PPU -> OAM[0xFD] = 0x%02X (Should be 0x22)", ppu1 -> oam[0xFD]);
    printf("PPU -> OAM[0xFE] = 0x%02X (Should be 0x33)", ppu1 -> oam[0xFE]);
    printf("PPU -> OAM[0xFD] = 0x%02X (Should be 0x44)", ppu1 -> oam[0xFF]);
    printf("Read on 0x2004 = 0x%02X, Read on PPU -> OAM[0x00] = 0x%02X",
           bus_cpu_read8(bus1, 0x2004), ppu1 -> oam[0x00]);


    // TEST CPU AND OAM DMA

    printf(" ====================== NROMMP TEST ====================== ");
    printf("Testing NROM-128 (16KiB PRG) mirroring $8000-$BFFF on $C000-$FFFF...");
    uint8_t v1 = bus_cpu_read8(bus1, 0x8000);
    uint8_t v2 = bus_cpu_read8(bus1, 0xBFFF);
    uint8_t v3 = bus_cpu_read8(bus1, 0xC000);
    uint8_t v4 = bus_cpu_read8(bus1, 0xFFFF);

    printf("$8000 = 0x%02X -> PGR-ROM = 0x%02X", v1, cart -> prg_rom[0]);
    printf("$BFFF = 0x%02X -> PGR-ROM = 0x%02X", v2, cart -> prg_rom[0x3FFF]);
    printf("$C000 = 0x%02X -> PGR-ROM = 0x%02X", v3, cart -> prg_rom[0]);
    printf("$FFFF = 0x%02X -> PGR-ROM = 0x%02X", v4, cart -> prg_rom[0x3FFF]);
    free_cartridge(cart);

    printf("Testing NROM-256 (32KiB PRG) mirroring $8000-bank0 on $C000-bank1...");
    cart = make_dummy(32, 8, false, false);

    uint8_t vA = bus_cpu_read8(bus1, 0x8000);
    uint8_t vB = bus_cpu_read8(bus1, 0xB000);
    uint8_t vC = bus_cpu_read8(bus1, 0xC000);
    uint8_t vD = bus_cpu_read8(bus1, 0xFFFF);

    printf("$8000 = 0x%02X -> PGR-ROM = 0x%02X", vA, cart -> prg_rom[0]);
    printf("$B000 = 0x%02X -> PGR-ROM = 0x%02X", vB, cart -> prg_rom[0x3000]);
    printf("$C000 = 0x%02X -> PGR-ROM = 0x%02X", vC, cart -> prg_rom[(KIB(16) + 0)]);
    printf("$FFFF = 0x%02X -> PGR-ROM = 0x%02X", vD, cart -> prg_rom[(KIB(16) + 0x3FFF)]);
    free_cartridge(cart);

    printf("Testing PRG-RAM (8Kib @ $6000-$7FFF) in R/W...");
    cart = make_dummy(16, 8, true, false);

    bus_cpu_write8(bus1, 0x6000, 0x5A);
    uint8_t r1 = bus_cpu_read8(bus1, 0x6000);
    printf("$6000 = 0x%02X (Should be 0x5A)", r1);

    bus_cpu_write8(bus1, 0x7FFF, 0x5A);
    uint8_t r2 = bus_cpu_read8(bus1, 0x7FFF);
    printf("$7FFF = 0x%02X (Should be 0x5A)", r2);
    free_cartridge(cart);

    printf("Testing no PRG-RAM (0Kib @ $6000-$7FFF) in R/W...");
    cart = make_dummy(16, 8, false, false);

    bus_cpu_write8(bus1, 0x6000, 0x5A);
    r1 = bus_cpu_read8(bus1, 0x6000);
    printf("$6000 = 0x%02X (Should be 0xFF)", r1);

    bus_cpu_write8(bus1, 0x7FFF, 0x5A);
    r2 = bus_cpu_read8(bus1, 0x7FFF);
    printf("$7FFF = 0x%02X (Should be 0xFF)", r2);
    free_cartridge(cart);

    printf("Testing CHR-ROM (read-only) via PPU ($0000–$1FFF)...");
    cart = make_dummy(16, 8, false, false); // CHR-ROM da 8 KiB

    r1 = ppu_read(bus1->ppu, 0x0000);
    r2 = ppu_read(bus1->ppu, 0x1FFE);
    uint8_t old1 = r1;

    ppu_write(bus1->ppu, 0x0000, (uint8_t)(r1 ^ 0xFF));  // ignorata su CHR-ROM
    uint8_t r1_after = ppu_read(bus1->ppu, 0x0000);

    printf("First read = 0x%02X, Old read = 0x%02X (Should be equal)\n", r1, old1);
    printf("First read (after write) = 0x%02X, Old read = 0x%02X (Should be equal)\n", r1_after, old1);
    printf("Read 0x1FFE = 0x%02X, CHR-ROM[0x1FFE] = 0x%02X (Should be equal)\n",
           r2, cart->chr_rom[0x1FFE]);
    free_cartridge(cart);

    printf("Testing CHR-RAM (writable) via PPU ($0000–$1FFF)...");
    cart = make_dummy(16, 8, false, true);

    ppu_write(bus1 -> ppu, 0x0000, 0x12);
    ppu_write(bus1 -> ppu, 0x1FFF, 0x34);

    uint8_t a = ppu_read(bus1 -> ppu, 0x0000);
    uint8_t b = ppu_read(bus1 -> ppu, 0x1FFF);

    printf("$0000 @ CHR_RAM = 0x%02X (Should be 0x12)", a);
    printf("$1FFF @ CHR_RAM = 0x%02X (Should be 0x12)", b);
    free_cartridge(cart);

    printf("Reset PRG sets (NROM-128 vs NROM-256)...");
    cart = make_dummy(16, 8, false, false);
    uint8_t x1 = bus_cpu_read8(bus1, 0x8000);
    uint8_t y1 = bus_cpu_read8(bus1, 0xC000);
    printf("$8000 (NROM-128) = 0x%02X, PRG-ROM[0] = 0x%02X (Should be equal)\n", x1, cart -> prg_rom[0]);
    printf("$C000 (NROM-128) = 0x%02X, PRG-ROM[0] = 0x%02X (Should be equal)\n", y1, cart -> prg_rom[0]);
    free_cartridge(cart);

    cart = make_dummy(32, 8, false, false);
    uint8_t x2 = bus_cpu_read8(bus1, 0x8000);
    uint8_t y2 = bus_cpu_read8(bus1, 0xC000);
    printf("$8000 (NROM-256) = 0x%02X, PRG-ROM[0] = 0x%02X (Should be equal)\n", x2, cart -> prg_rom[0]);
    printf("$C000 (NROM-256) = 0x%02X, PRG-ROM[16KiB] = 0x%02X (Should be equal)\n", y2, cart -> prg_rom[KIB(16) + 0]);
    free_cartridge(cart);

    printf("Precise offsets in PRG Slots (NROM-128 vs NROM-256)...");
    cart = make_dummy(32, 8, false, false);

    uint8_t a1 = bus_cpu_read8(bus1, 0x8123);           //bank0 + 0x0123
    uint8_t b1 = bus_cpu_read8(bus1, 0xBFFF);           //bank0 + 0x3FFF
    uint8_t c1 = bus_cpu_read8(bus1, 0xC000 + 0x0456);  //bank1 + 0x0456
    uint8_t d1 = bus_cpu_read8(bus1, 0xFFFF);           //bank1 + 0x3FFF
    printf("$8123 (NROM-256) = 0x%02X, PRG-ROM[0x0123] = 0x%02X (Should be equal)\n", a1, cart -> prg_rom[0x0123]);
    printf("$BFFF (NROM-256) = 0x%02X, PRG-ROM[0x3FFF] = 0x%02X (Should be equal)\n", b1, cart -> prg_rom[0x3FFF]);
    printf("$C456 (NROM-256) = 0x%02X, PRG-ROM[KIB(16) + 0x0456] = 0x%02X (Should be equal)\n", c1, cart -> prg_rom[KIB(16) + 0x0456]);
    printf("$FFFF (NROM-256) = 0x%02X, PRG-ROM[KIB(16) + 0x3FFF] = 0x%02X (Should be equal)\n", d1, cart -> prg_rom[KIB(16) + 0x3FFF]);

















    // free_cartridge(cart);
    mapper_destroy(mapper1);
    free(bus1);
    free(ppu1);
    log_stop();
    return 0;
}
