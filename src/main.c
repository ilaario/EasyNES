#include "headers/main.h"

cartridge pCartridge;

int main(int argc, char const *argv[]) {
    log_init("log/easynes.log");

    if (argc != 2) {
        fprintf(stderr, "Usage: <game>.nes\n");
        return EXIT_FAILURE;
    }

    // pCartridge = read_allocate_cartridge(argv[1]);

    // print_info(pCartridge);

    // printf("Cartridge read - DONE");

    bus CPUbus = (bus)malloc(sizeof(struct CPUBus));
    ppu ppu1 = (ppu)malloc(sizeof(struct PPU));
    mapper mapper1 = NULL;
    controller pad1 = NULL;
    controller pad2 = NULL;

    ppu_init(ppu1, mapper1, 0);
    bus_init(CPUbus, ppu1, mapper1, pad1, pad2);

    printf(" ======================= RAM TEST ======================= ");
    printf("Writing 0xAA at position 0x0000 to check mirroring RAM... ");

    bus_cpu_write8(CPUbus, 0x0000, 0xAA);

    printf("Ram at pos 0x0000 = 0x%02X", bus_cpu_read8(CPUbus, 0x0000));
    printf("Ram at pos 0x0800 = 0x%02X", bus_cpu_read8(CPUbus, 0x0800));
    printf("Ram at pos 0x1000 = 0x%02X", bus_cpu_read8(CPUbus, 0x1000));
    printf("Ram at pos 0x1800 = 0x%02X", bus_cpu_read8(CPUbus, 0x1800));

    printf(" ======================= PPU TEST ======================= ");
    printf("Writing 0x11 at position 0x2000 to check mirroring PPU... ");

    // Scrivo in 0x2000 (cioè registro 0)
    bus_cpu_write8(CPUbus, 0x2000, 0x11);

    printf("PPU at reg 0x2008 = 0x%02X", bus_cpu_read8(CPUbus, 0x2008));
    printf("PPU at reg 0x3FF8 = 0x%02X", bus_cpu_read8(CPUbus, 0x3FF8));

    printf("Setting VBlank = 1...");
    ppu1 -> ppustatus |= 0x80;
    printf("VBlank set to 1");
    printf("Side-effect on $2002 = %s (Should be true)", (bus_cpu_read8(CPUbus, 0x2002) & 0x80) != 0 ? "True" : "False");
    printf("Flag cleared = %s (Should be true)", (ppu1 -> ppustatus & 0x80) == 0 ? "True" : "False");

    printf("Writing on CIRAM...");
    bus_cpu_write8(CPUbus, 0x2006, 0x20);          // high
    bus_cpu_write8(CPUbus, 0x2006, 0x00);          // low → v=0x2000
    bus_cpu_write8(CPUbus, 0x2007, 0x12);          // CIRAM[0]=0x12
    printf("Written 0x12 in $2007 after moving address");

    printf("Setting address to the same point as before...");
    bus_cpu_write8(CPUbus, 0x2006, 0x20);          // high
    bus_cpu_write8(CPUbus, 0x2006, 0x00);          // low → v=0x2000
    bus_cpu_read8(CPUbus, 0x2007);
    printf("First read ignored, second read = 0x%02X", bus_cpu_read8(CPUbus, 0x2007));

    printf("Testing VBlank without NMI...");
    ppu_reset(ppu1);
    bus_cpu_write8(CPUbus, 0x2000, 0x00);
    DEBUG_goto_scanline_dot(ppu1, 241, 1);
    printf("VBlank = %d (Should be 1 at (241,1) without NMI)", (ppu1 -> ppustatus & 0x80) != 0);
    printf("NMI = %s (NMI should not be pending if NMI is off)", ppu1 -> nmi_pending ? "True" : "False");

    printf("Testing VBlank with NMI...");
    ppu_reset(ppu1);
    bus_cpu_write8(CPUbus, 0x2000, 0x80);
    DEBUG_goto_scanline_dot(ppu1, 241, 1);
    printf("VBlank = %s (Should be true at (241,1) with NMI)", (ppu1 -> ppustatus & 0x80) != 0 ? "True" : "False");
    printf("NMI = %s (NMI should be pending if NMI is on)", ppu1 -> nmi_pending ? "True" : "False");
    ppu_clear_nmi(ppu1);
    printf("NMI = %s (NMI should not be pending after clear)", ppu1 -> nmi_pending ? "True" : "False");

    printf("Testing Pre-Render...");
    ppu_reset(ppu1);
    bus_cpu_write8(CPUbus, 0x2000, 0x80);
    DEBUG_goto_scanline_dot(ppu1, 241, 1);
    printf("Simulated to (241, 1), now in VBlank");
    printf("VBlank = %s", (ppu1 -> ppustatus & 0x80) != 0 ? "True" : "False");
    printf("Starting Pre-Render...");
    DEBUG_goto_scanline_dot(ppu1, 260, 1);
    printf("VBlank = %s (VBlank should be false at pre-render)", (ppu1 -> ppustatus & 0x80) != 0 ? "True" : "False");

    printf("Testing Side Effect...");
    ppu_reset(ppu1);
    bus_cpu_write8(CPUbus, 0x2000, 0x00);
    DEBUG_goto_scanline_dot(ppu1, 241, 1);
    printf("Read from $2002 = 0x%02X", (bus_cpu_read8(CPUbus, 0x2002) & 0x80));
    printf("VBlank = %s (VBlank should be false after read in $2002)", (ppu1 -> ppustatus & 0x80) != 0 ? "True" : "False");
    printf("Write toggle = %s (Should be false after read in $2002)", ppu1 -> w != 0 ? "True" : "False");

    printf("Testing increment of Coarse X...");
    ppu_reset(ppu1);
    bus_cpu_write8(CPUbus, 0x2001, 0x08);
    ppu1 -> v = 0x0005;
    DEBUG_goto_scanline_dot(ppu1, 0, 8);
    printf("PPU -> V = 0x%04X (Should be 0x0006)", ppu1 -> v);
    ppu_reset(ppu1);
    bus_cpu_write8(CPUbus, 0x2001, 0x08);
    ppu1 -> v = 0x001F;
    DEBUG_goto_scanline_dot(ppu1, 0, 8);      // primo boundary (wrap → 0x0400)
    printf("PPU -> V = 0x%04X (Should be 0x0400)", ppu1 -> v);
    DEBUG_goto_scanline_dot(ppu1, 0, 16);     // secondo boundary (→ 0x0401)
    printf("PPU -> V = 0x%04X (Should be 0x0401)", ppu1 -> v);

    printf("Testing vertical increment...");

// ===== Caso 1: fineY < 7 ⇒ fineY++ =====
    ppu_reset(ppu1);
    bus_cpu_write8(CPUbus, 0x2001, 0x00);               // rendering OFF (niente inc X)
    ppu1->v = (0x3000) | (10 << 5) | 0x00;                          // fineY=3, coarseY=10, coarseX=0, NTX/NTY=0
    DEBUG_goto_scanline_dot(ppu1, 0, 255);     // fermati PRIMA del 256

    bus_cpu_write8(CPUbus, 0x2001, 0x08);            // rendering ON solo per il ciclo 256
    step_one_ppu_cycle(ppu1);                               // esegue dot=256 ⇒ SOLO increment_y
    printf("PPU -> V = 0x%04X (Should be 0x%04X)\n",
           ppu1->v, (0x4000 | (10 << 5)));                       // 0x4140 atteso

// ===== Caso 2: fineY = 7 ⇒ carry su coarseY =====
    ppu_reset(ppu1);
    bus_cpu_write8(CPUbus, 0x2001, 0x00);            // rendering OFF
    ppu1->v = (0x7000) | (12 << 5) | 0x00;                       // fineY=7, coarseY=12, coarseX=0
    DEBUG_goto_scanline_dot(ppu1, 0, 255);

    bus_cpu_write8(CPUbus, 0x2001, 0x08);             // ON solo per il 256
    step_one_ppu_cycle(ppu1);                                // dot=256 ⇒ fineY=0, coarseY=13
    printf("PPU -> V = 0x%04X (Should be 0x%04X)",
           ppu1->v, (0x0000 | (13 << 5)));

// ===== Caso 3: coarseY = 29 ⇒ 0 e toggle NTY =====
    ppu_reset(ppu1);
    bus_cpu_write8(CPUbus, 0x2001, 0x08);
    ppu1 -> v = 0x7000 | (29 << 5);
    DEBUG_goto_scanline_dot(ppu1, 0, 256);
    printf("PPU -> V = 0x%04X (Should be 0x0000)\n",
           (ppu1->v & 0x03E0));
    printf("NTY = %s (Should be false)\n",
           (ppu1->v & 0x0800) == 0 ? "True" : "False");

// ===== Caso 4: coarseY = 31 ⇒ 0 senza toggle NTY =====
    ppu_reset(ppu1);
    bus_cpu_write8(CPUbus, 0x2001, 0x08);
    ppu1 -> v = 0x7000 | (31 << 5);
    DEBUG_goto_scanline_dot(ppu1, 0, 256);
    printf("PPU -> V = 0x%04X (Should be 0x0000)\n",
           (ppu1->v & 0x03E0));
    printf("NTY = %s (Should be true)\n",
           (ppu1->v & 0x0800) == 0 ? "True" : "False");

    printf("Vertical copy on pre-render...");
    ppu_reset(ppu1);
    bus_cpu_write8(CPUbus, 0x2001, 0x08);
    ppu1 -> t = 0x7000 | 0x0800 | (17 << 5);
    DEBUG_goto_scanline_dot(ppu1, 261, 304);
    printf("PPU -> V == PPU -> T is %s (Should be true)",
           (ppu1 -> v & 0x7BE0) == (ppu1 -> t & 0x7BE0) ? "True" : "False");

    printf("Testing full scroll sequence on the same line...");
    ppu_reset(ppu1);
    bus_cpu_write8(CPUbus, 0x2001, 0x00);
    DEBUG_goto_scanline_dot(ppu1, 5, 7);
    ppu1->v = 0x001F;
    bus_cpu_write8(CPUbus, 0x2001, 0x08);
    step_one_ppu_cycle(ppu1);
    printf("PPU -> V = 0x%04X (Should be 0x0400)\n", ppu1->v);

    printf(" ====================== OAMDMA TEST ====================== ");
    printf("Testing OAMADDR and OAMDATA sequence...");
    ppu_reset(ppu1);

    bus_cpu_write8(CPUbus, 0x2003, 0xFC);      // OAMADDR=0xFC
    bus_cpu_write8(CPUbus, 0x2004, 0x11);
    bus_cpu_write8(CPUbus, 0x2004, 0x22);
    bus_cpu_write8(CPUbus, 0x2004, 0x33);
    bus_cpu_write8(CPUbus, 0x2004, 0x44);

    printf("PPU -> OAMADDR   = 0x%02X (Should be 0x00)", ppu1 -> oamaddr);
    printf("PPU -> OAM[0xFC] = 0x%02X (Should be 0x11)", ppu1 -> oam[0xFC]);
    printf("PPU -> OAM[0xFD] = 0x%02X (Should be 0x22)", ppu1 -> oam[0xFD]);
    printf("PPU -> OAM[0xFE] = 0x%02X (Should be 0x33)", ppu1 -> oam[0xFE]);
    printf("PPU -> OAM[0xFD] = 0x%02X (Should be 0x44)", ppu1 -> oam[0xFF]);
    printf("Read on 0x2004 = 0x%02X, Read on PPU -> OAM[0x00] = 0x%02X",
           bus_cpu_read8(CPUbus, 0x2004), ppu1 -> oam[0x00]);







    // free_cartridge(pCartridge);
    free(CPUbus);
    free(ppu1);
    log_stop();
    return 0;
}
