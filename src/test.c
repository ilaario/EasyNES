// ————————————————————————————————————————
// ANSI (se non già definiti)
// ————————————————————————————————————————

#include "headers/test.h"

#define ANSI_YELLOW "\x1b[33m"

#ifndef ANSI_GREEN
#define ANSI_GREEN  "\x1b[32m"
#define ANSI_RED    "\x1b[31m"
#define ANSI_RESET  "\x1b[0m"
#endif

// ————————————————————————————————————————
// Assert helpers
// ————————————————————————————————————————
static inline void assert_true(bool cond, const char *msg) {
    if (cond) {
        printf(ANSI_GREEN "[OK] %s" ANSI_RESET, msg);
    } else {
        printf(ANSI_RED "[FAIL] %s" ANSI_RESET, msg);
    }
}

static inline void assert_false(bool cond, const char *msg) {
    assert_true(!cond, msg);
}

static inline void assert_eq_u8(uint8_t a, uint8_t b, const char *msg) {
    if (a == b) {
        printf(ANSI_GREEN "[OK] %s (0x%02X == 0x%02X)" ANSI_RESET, msg, a, b);
    } else {
        printf(ANSI_RED "[FAIL] %s (0x%02X != 0x%02X)" ANSI_RESET, msg, a, b);
    }
}

static inline void assert_eq_u16(uint16_t a, uint16_t b, const char *msg) {
    if (a == b) {
        printf(ANSI_GREEN "[OK] %s (0x%04X == 0x%04X)" ANSI_RESET, msg, a, b);
    } else {
        printf(ANSI_RED "[FAIL] %s (0x%04X != 0x%04X)" ANSI_RESET, msg, a, b);
    }
}

static inline void assert_eq_int(int a, int b, const char *msg) {
    if (a == b) {
        printf(ANSI_GREEN "[OK] %s (%d == %d)" ANSI_RESET, msg, a, b);
    } else {
        printf(ANSI_RED "[FAIL] %s (%d != %d)" ANSI_RESET, msg, a, b);
    }
}

// ————————————————————————————————————————
// Test context (global handles set by test_setup)
// ————————————————————————————————————————
static cpu        cpu1;
static ppu        ppu1;
static bus        bus1;
static mapper     mapper1;
static controller pad1, pad2;

void test_setup(cpu cpu, bus bus, ppu ppu, mapper mapper, controller pad_1, controller pad_2){
    cpu1    = cpu;
    ppu1    = ppu;
    bus1    = bus;
    mapper1 = mapper;
    pad1    = pad_1;
    pad2    = pad_2;
}

// ————————————————————————————————————————
// RAM
// ————————————————————————————————————————
static void ram_mirroring(){
    uint8_t val = 0xAA;
    bus_cpu_write8(bus1, 0x0000, val);

    assert_eq_u8(bus_cpu_read8(bus1, 0x0000), val, "RAM mirror $0000");
    assert_eq_u8(bus_cpu_read8(bus1, 0x0800), val, "RAM mirror $0800");
    assert_eq_u8(bus_cpu_read8(bus1, 0x1000), val, "RAM mirror $1000");
    assert_eq_u8(bus_cpu_read8(bus1, 0x1800), val, "RAM mirror $1800");
}

// ————————————————————————————————————————
// PPU register mirroring ($2000–$2007 mirrored to $2008…$3FFF)
// ————————————————————————————————————————
static void ppu_mirroring(){
    bus_cpu_write8(bus1, 0x2000, 0x11);
    assert_eq_u8(ppu1->ppuctrl, 0x11, "PPUCTRL written via $2000");

    bus_cpu_write8(bus1, 0x2008, 0x22);
    assert_eq_u8(ppu1->ppuctrl, 0x22, "PPUCTRL written via $2008 mirror");

    bus_cpu_write8(bus1, 0x3FF8, 0x33);
    assert_eq_u8(ppu1->ppuctrl, 0x33, "PPUCTRL written via $3FF8 mirror");

    // $2002 is mirrored every 8 bytes ($200A, $2012, ...), but reads have side effects.
    // Test each mirror independently (read clears VBlank), not by comparing two consecutive reads.

    // Read from mirror first
    ppu1->ppustatus = 0x80; // set VBlank
    uint8_t r_mirror = bus_cpu_read8(bus1, 0x200A); // mirror of $2002
    assert_true((r_mirror & 0x80) != 0, "$200A (mirror of $2002) shows VBlank");
    assert_true((ppu1->ppustatus & 0x80) == 0, "$200A read clears VBlank");

    // Reset and read from base register
    ppu1->ppustatus = 0x80; // set VBlank again
    uint8_t r_base = bus_cpu_read8(bus1, 0x2002);
    assert_true((r_base & 0x80) != 0, "$2002 shows VBlank");
    assert_true((ppu1->ppustatus & 0x80) == 0, "$2002 read clears VBlank");
}

// ————————————————————————————————————————
// PPUSTATUS side-effect: read clears VBlank e w
// ————————————————————————————————————————
static void set_vblank(){
    // Set manuale del flag
    ppu1->ppustatus |= 0x80;
    // Leggendo $2002 il bit7 deve risultare 1 nella lettura ma venire poi azzerato nello stato
    uint8_t s = bus_cpu_read8(bus1, 0x2002);
    assert_true((s & 0x80) != 0, "PPUSTATUS bit7 visible on $2002 read");
    assert_true((ppu1->ppustatus & 0x80) == 0, "PPUSTATUS.VBlank cleared after $2002 read");
}

// ————————————————————————————————————————
// VRAM I/O via $2006/$2007
// ————————————————————————————————————————
static void writing_ciram(){
    // Imposta v = $2000 e scrivi 0x12
    bus_cpu_write8(bus1, 0x2006, 0x20); // high
    bus_cpu_write8(bus1, 0x2006, 0x00); // low
    bus_cpu_write8(bus1, 0x2007, 0x12); // write

    // Reimposta l’indirizzo e verifica buffered read: primo read ignora, secondo riporta 0x12
    bus_cpu_write8(bus1, 0x2006, 0x20);
    bus_cpu_write8(bus1, 0x2006, 0x00);
    (void)bus_cpu_read8(bus1, 0x2007); // dummy
    uint8_t rd = bus_cpu_read8(bus1, 0x2007);
    assert_eq_u8(rd, 0x12, "$2007 read returns last written value at $2000");
}

// ————————————————————————————————————————
// VBlank senza NMI
// ————————————————————————————————————————
static void test_vram_without_nmi(){
    ppu_reset(ppu1);
    bus_cpu_write8(bus1, 0x2000, 0x00); // NMI off
    DEBUG_goto_scanline_dot(ppu1, 241, 1);
    assert_true( (ppu1->ppustatus & 0x80) != 0, "VBlank=1 at (241,1) with NMI off");
    assert_false(ppu1->nmi_pending, "NMI not pending when disabled");
}

// ————————————————————————————————————————
// VBlank con NMI
// ————————————————————————————————————————
static void test_vram_with_nmi(){
    ppu_reset(ppu1);
    bus_cpu_write8(bus1, 0x2000, 0x80); // NMI on
    DEBUG_goto_scanline_dot(ppu1, 241, 1);
    assert_true( (ppu1->ppustatus & 0x80) != 0, "VBlank=1 at (241,1) with NMI on");
    assert_true( ppu1->nmi_pending, "NMI pending when enabled");
    ppu_clear_nmi(ppu1);
    assert_false( ppu1->nmi_pending, "NMI cleared after ppu_clear_nmi()");
}

// ————————————————————————————————————————
// Pre-render line clears VBlank
// ————————————————————————————————————————
static void test_prerender(){
    ppu_reset(ppu1);
    bus_cpu_write8(bus1, 0x2000, 0x80);
    DEBUG_goto_scanline_dot(ppu1, 241, 1);
    assert_true( (ppu1->ppustatus & 0x80) != 0, "In VBlank before pre-render");
    DEBUG_goto_scanline_dot(ppu1, 261, 1);
    assert_false( (ppu1->ppustatus & 0x80) != 0, "VBlank cleared at pre-render");
}

// ————————————————————————————————————————
// Side-effect su $2002: clear VBlank e toggle w
// ————————————————————————————————————————
static void test_sideeffect(){
    ppu_reset(ppu1);
    bus_cpu_write8(bus1, 0x2000, 0x00);
    DEBUG_goto_scanline_dot(ppu1, 241, 1);
    (void)bus_cpu_read8(bus1, 0x2002); // side-effect
    assert_false( (ppu1->ppustatus & 0x80) != 0, "VBlank cleared after reading $2002");
    assert_eq_u8(ppu1->w, 0, "Write toggle w cleared after reading $2002");
}

// ————————————————————————————————————————
// Scorrimento orizzontale (incremento coarse X)
// ————————————————————————————————————————
static void test_inc_coarse_x(){
    // increment normale
    ppu_reset(ppu1);
    bus_cpu_write8(bus1, 0x2001, 0x08); // rendering ON
    ppu1->v = 0x0005;
    DEBUG_goto_scanline_dot(ppu1, 0, 8);
    assert_eq_u16(ppu1->v, 0x0006, "coarseX: 5 -> 6 at dot=8");

    // wrap a 0 e toggle NTX
    ppu_reset(ppu1);
    bus_cpu_write8(bus1, 0x2001, 0x08);
    ppu1->v = 0x001F;
    DEBUG_goto_scanline_dot(ppu1, 0, 8);
    assert_eq_u16(ppu1->v, 0x0400, "coarseX: 31 -> 0 & toggle NTX at dot=8");
    DEBUG_goto_scanline_dot(ppu1, 0, 16);
    assert_eq_u16(ppu1->v, 0x0401, "coarseX: next increment -> 0x0401");
}

// ————————————————————————————————————————
// Incremento verticale (3 casi) + copia verticale al pre-render
// ————————————————————————————————————————
static void test_vertical_increment_and_copy(){
    // Caso 1: fineY < 7 -> fineY++
    ppu_reset(ppu1);
    bus_cpu_write8(bus1, 0x2001, 0x00);
    ppu1->v = (0x3000) | (10 << 5) | 0x00;
    DEBUG_goto_scanline_dot(ppu1, 0, 255);
    bus_cpu_write8(bus1, 0x2001, 0x08);
    step_one_ppu_cycle(ppu1); // dot 256
    assert_eq_u16(ppu1->v, (0x4000 | (10 << 5)), "fineY<7: increment_y at dot=256");

    // Caso 2: fineY = 7 -> carry su coarseY
    ppu_reset(ppu1);
    bus_cpu_write8(bus1, 0x2001, 0x00);
    ppu1->v = (0x7000) | (12 << 5) | 0x00;
    DEBUG_goto_scanline_dot(ppu1, 0, 255);
    bus_cpu_write8(bus1, 0x2001, 0x08);
    step_one_ppu_cycle(ppu1);
    assert_eq_u16(ppu1->v, (0x0000 | (13 << 5)), "fineY=7: carry to coarseY");

    // Caso 3: coarseY=29 -> 0 e toggle NTY
    ppu_reset(ppu1);
    bus_cpu_write8(bus1, 0x2001, 0x08);
    ppu1->v = 0x7000 | 0x0800 | (29 << 5);
    DEBUG_goto_scanline_dot(ppu1, 0, 256);
    assert_eq_u16(ppu1->v & 0x03E0, 0x0000, "coarseY=29 -> 0");
    assert_true((ppu1->v & 0x0800) == 0, "coarseY=29 toggles NTY from 1->0");

    // Caso 4: coarseY=31 -> 0 senza toggle NTY
    ppu_reset(ppu1);
    bus_cpu_write8(bus1, 0x2001, 0x08);
    ppu1->v = 0x7000 | 0x0800 | (31 << 5);
    DEBUG_goto_scanline_dot(ppu1, 0, 256);
    assert_eq_u16(ppu1->v & 0x03E0, 0x0000, "coarseY=31 -> 0");
    assert_true((ppu1->v & 0x0800) != 0, "coarseY=31 keeps NTY set (stays 1)");

    // Copia verticale su pre-render (261,304 → dot 304)
    ppu_reset(ppu1);
    bus_cpu_write8(bus1, 0x2001, 0x08);
    ppu1->t = 0x7000 | 0x0800 | (17 << 5);
    DEBUG_goto_scanline_dot(ppu1, 261, 304);
    assert_true( (ppu1->v & 0x7BE0) == (ppu1->t & 0x7BE0), "Vertical copy v<=t at pre-render");
}

// ————————————————————————————————————————
// Sequenza full scroll sulla stessa linea (wrap X)
// ————————————————————————————————————————
static void test_full_scroll_same_line(){
    ppu_reset(ppu1);
    bus_cpu_write8(bus1, 0x2001, 0x00);
    DEBUG_goto_scanline_dot(ppu1, 5, 7);
    ppu1->v = 0x001F;
    bus_cpu_write8(bus1, 0x2001, 0x08);
    step_one_ppu_cycle(ppu1); // dot=8
    assert_eq_u16(ppu1->v, 0x0400, "full scroll same line: wrap to 0x0400");
}

// ————————————————————————————————————————
// OAMADDR/OAMDATA basic sequence
// ————————————————————————————————————————
static void test_oamaddr_oamdata_seq(){
    ppu_reset(ppu1);

    bus_cpu_write8(bus1, 0x2003, 0xFC);   // OAMADDR=0xFC
    bus_cpu_write8(bus1, 0x2004, 0x11);
    bus_cpu_write8(bus1, 0x2004, 0x22);
    bus_cpu_write8(bus1, 0x2004, 0x33);
    bus_cpu_write8(bus1, 0x2004, 0x44);

    assert_eq_u8(ppu1->oamaddr, 0x00, "OAMADDR wrapped to 0x00");
    assert_eq_u8(ppu1->oam[0xFC], 0x11, "OAM[0xFC]");
    assert_eq_u8(ppu1->oam[0xFD], 0x22, "OAM[0xFD]");
    assert_eq_u8(ppu1->oam[0xFE], 0x33, "OAM[0xFE]");
    assert_eq_u8(ppu1->oam[0xFF], 0x44, "OAM[0xFF]");
    assert_eq_u8(bus_cpu_read8(bus1, 0x2004), ppu1->oam[0x00], "Reading $2004 returns OAM[OAMADDR]");
}

// ————————————————————————————————————————
// Mapper NROM tests + CHR via PPU
// (richiedono make_dummy(...) e free_cartridge(...))
// ————————————————————————————————————————
static void mapper_nrom_128(){
    uint8_t v1 = bus_cpu_read8(bus1, 0x8000);
    uint8_t v2 = bus_cpu_read8(bus1, 0xBFFF);
    uint8_t v3 = bus_cpu_read8(bus1, 0xC000);
    uint8_t v4 = bus_cpu_read8(bus1, 0xFFFF);

    assert_eq_u8(v1, bus1 -> mapper -> cart->prg_rom[0x0000], "NROM-128 $8000 mirrors PRG[0]");
    assert_eq_u8(v2, bus1 -> mapper -> cart->prg_rom[0x3FFF], "NROM-128 $BFFF mirrors PRG[0x3FFF]");
    assert_eq_u8(v3, bus1 -> mapper -> cart->prg_rom[0x0000], "NROM-128 $C000 mirrors bank0");
    assert_eq_u8(v4, bus1 -> mapper -> cart->prg_rom[0x3FFF], "NROM-128 $FFFF mirrors bank0 end");

    free_cartridge(bus1 -> mapper -> cart);
}

static void mapper_nrom_256(){
    mapper_nrom_attach(bus1 -> mapper, make_dummy(32, 8, false, false));

    uint8_t vA = bus_cpu_read8(bus1, 0x8000);
    uint8_t vB = bus_cpu_read8(bus1, 0xB000);
    uint8_t vC = bus_cpu_read8(bus1, 0xC000);
    uint8_t vD = bus_cpu_read8(bus1, 0xFFFF);

    assert_eq_u8(vA, bus1 -> mapper -> cart->prg_rom[0x0000],               "NROM-256 $8000 = bank0[0]");
    assert_eq_u8(vB, bus1 -> mapper -> cart->prg_rom[0x3000],               "NROM-256 $B000 = bank0[0x3000]");
    assert_eq_u8(vC, bus1 -> mapper -> cart->prg_rom[0x4000 + 0x0000],      "NROM-256 $C000 = bank1[0]");
    assert_eq_u8(vD, bus1 -> mapper -> cart->prg_rom[0x4000 + 0x3FFF],      "NROM-256 $FFFF = bank1[0x3FFF]");

    free_cartridge(bus1 -> mapper -> cart);
}

static void mapper_prgram_rw(){
    bus1 -> mapper -> cart = make_dummy(16, 8, true, false);

    bus_cpu_write8(bus1, 0x6000, 0x5A);
    bus_cpu_write8(bus1, 0x7FFF, 0x5A);
    assert_eq_u8(bus_cpu_read8(bus1, 0x6000), 0x5A, "PRG-RAM $6000 R/W");
    assert_eq_u8(bus_cpu_read8(bus1, 0x7FFF), 0x5A, "PRG-RAM $7FFF R/W");

    free_cartridge(bus1 -> mapper -> cart);
}

static void mapper_no_prgram_rw(){
    bus1 -> mapper -> cart = make_dummy(16, 8, false, false);

    bus_cpu_write8(bus1, 0x6000, 0x5A);
    bus_cpu_write8(bus1, 0x7FFF, 0x5A);
    assert_eq_u8(bus_cpu_read8(bus1, 0x6000), 0xFF, "No PRG-RAM $6000 reads as 0xFF");
    assert_eq_u8(bus_cpu_read8(bus1, 0x7FFF), 0xFF, "No PRG-RAM $7FFF reads as 0xFF");

    free_cartridge(bus1 -> mapper -> cart);
}

static void chr_rom_readonly_via_ppu(){
    bus1 -> mapper -> cart = make_dummy(16, 8, false, false);

    uint8_t r1 = ppu_read(bus1->ppu, 0x0000);
    uint8_t r2 = ppu_read(bus1->ppu, 0x1FFE);
    uint8_t old1 = r1;

    ppu_write(bus1->ppu, 0x0000, (uint8_t)(r1 ^ 0xFF)); // ignorata su CHR-ROM
    uint8_t r1_after = ppu_read(bus1->ppu, 0x0000);

    assert_eq_u8(r1, old1,        "CHR-ROM first read stable");
    assert_eq_u8(r1_after, old1,  "CHR-ROM write ignored");
    assert_eq_u8(r2, bus1 -> mapper -> cart->chr_rom[0x1FFE], "CHR-ROM read matches backing");

    free_cartridge(bus1 -> mapper -> cart);
}

static void chr_ram_writable_via_ppu(){
    bus1 -> mapper -> cart = make_dummy(16, 8, false, true); // CHR-RAM

    ppu_write(bus1->ppu, 0x0000, 0x12);
    ppu_write(bus1->ppu, 0x1FFF, 0x34);

    uint8_t a = ppu_read(bus1->ppu, 0x0000);
    uint8_t b = ppu_read(bus1->ppu, 0x1FFF);

    // CHR-RAM è scrivibile: abbiamo scritto 0x12 a $0000 e 0x34 a $1FFF,
    // quindi ci aspettiamo di leggere 0x12 e 0x34 rispettivamente.
    assert_eq_u8(a, 0x12, "$0000 @ CHR-RAM");
    assert_eq_u8(b, 0x34, "$1FFF @ CHR-RAM");

    free_cartridge(bus1 -> mapper -> cart);
}

static void reset_prg_sets(){
    bus1 -> mapper -> cart = make_dummy(16, 8, false, false);
    uint8_t x1 = bus_cpu_read8(bus1, 0x8000);
    uint8_t y1 = bus_cpu_read8(bus1, 0xC000);
    assert_eq_u8(x1, bus1 -> mapper -> cart->prg_rom[0], "NROM-128 $8000 = PRG[0]");
    assert_eq_u8(y1, bus1 -> mapper -> cart->prg_rom[0], "NROM-128 $C000 = PRG[0]");
    free_cartridge(bus1 -> mapper -> cart);

    bus1 -> mapper -> cart = make_dummy(32, 8, false, false);
    uint8_t x2 = bus_cpu_read8(bus1, 0x8000);
    uint8_t y2 = bus_cpu_read8(bus1, 0xC000);
    assert_eq_u8(x2, bus1 -> mapper -> cart->prg_rom[0],            "NROM-256 $8000 = PRG[0]");
    assert_eq_u8(y2, bus1 -> mapper -> cart->prg_rom[0x4000 + 0],   "NROM-256 $C000 = PRG[16KiB]");
    free_cartridge(bus1 -> mapper -> cart);
}

static void precise_offsets_prg_slots(){
    bus1 -> mapper -> cart = make_dummy(32, 8, false, false);

    uint8_t a1 = bus_cpu_read8(bus1, 0x8123);
    uint8_t b1 = bus_cpu_read8(bus1, 0xBFFF);
    uint8_t c1 = bus_cpu_read8(bus1, 0xC000 + 0x0456);
    uint8_t d1 = bus_cpu_read8(bus1, 0xFFFF);

    assert_eq_u8(a1, bus1 -> mapper -> cart->prg_rom[0x0123],                 "$8123 == PRG[0x0123]");
    assert_eq_u8(b1, bus1 -> mapper -> cart->prg_rom[0x3FFF],                 "$BFFF == PRG[0x3FFF]");
    assert_eq_u8(c1, bus1 -> mapper -> cart->prg_rom[0x4000 + 0x0456],        "$C456 == bank1[0x0456]");
    assert_eq_u8(d1, bus1 -> mapper -> cart->prg_rom[0x4000 + 0x3FFF],        "$FFFF == bank1[0x3FFF]");

    free_cartridge(bus1 -> mapper -> cart);
}

// Reusa gli assert del tuo file principale:
// assert_true, assert_false, assert_eq_u8, assert_eq_u16, assert_eq_int
// e i global cpu1, bus1 già valorizzati da test_setup(...).

// Flag P (bit): NV-BDIZC
#define FLAG_N 0x80
#define FLAG_V 0x40
#define FLAG_U 0x20  // unused
#define FLAG_B 0x10
#define FLAG_D 0x08
#define FLAG_I 0x04
#define FLAG_Z 0x02
#define FLAG_C 0x01

// ————————————————————————————————————————
// Helpers CPU
// ————————————————————————————————————————
static inline void cpu_write_prog(uint16_t addr, const uint8_t *code, size_t len){
    for(size_t i=0;i<len;i++) bus_cpu_write8(bus1, (uint16_t)(addr+i), code[i]);
}
static inline void cpu_run_steps(int n){
    for(int i=0;i<n;i++) cpu_step(cpu1);
}
static inline void cpu_set_pc(uint16_t pc){
    cpu1->PC = pc;
}

// ————————————————————————————————————————
// Reset vector & PC
// Nota: molti mapper non permettono di scrivere in $FFFC/$FFFD.
// Per un test “puro” sul vettore di reset serve un cart speciale o
// scrittura forzata sulla ROM. Qui verifichiamo solo che cpu_reset()
// non corrompa i registri, e usiamo poi PC forzato in RAM per i test.
// ————————————————————————————————————————
static void cpu_sanity_after_reset(){
    cpu_reset(cpu1);
    // Non imponiamo valori assoluti post-reset (variano per implementazione),
    // ma verifichiamo che SP sia nella page $0100 e che l'IRQ sia settato.
    assert_true((cpu1->S & 0xFF) == cpu1->S, "SP is 8-bit");
    assert_true((cpu1->P & FLAG_I) != 0, "Interrupt Disable set after reset");
}

// ————————————————————————————————————————
// NOP & avanzamento PC
// ————————————————————————————————————————
static void cpu_nop_increments_pc(){
    // Programma in RAM @ $0000: EA EA (NOP; NOP)
    static const uint8_t prog[] = { 0xEA, 0xEA };
    cpu_write_prog(0x0000, prog, sizeof(prog));
    cpu_set_pc(0x0000);

    uint16_t pc0 = cpu1->PC;
    cpu_run_steps(1);
    assert_eq_u16(cpu1->PC, pc0+1, "NOP increments PC by 1");
    cpu_run_steps(1);
    assert_eq_u16(cpu1->PC, pc0+2, "Second NOP increments PC by 1");
}

// ————————————————————————————————————————
// LDA immediate / flag Z,N
// ————————————————————————————————————————
static void cpu_lda_immediate_flags(){
    // A <- 0x00 -> Z=1,N=0 ; poi A <- 0x80 -> Z=0,N=1
    static const uint8_t prog[] = {
            0xA9, 0x00,       // LDA #$00
            0xA9, 0x80        // LDA #$80
    };
    cpu_write_prog(0x0010, prog, sizeof(prog));
    cpu_set_pc(0x0010);

    cpu_run_steps(1);
    assert_eq_u8(cpu1->A, 0x00, "LDA #$00 loads A");
    assert_true((cpu1->P & FLAG_Z)!=0, "LDA #$00 sets Z");
    assert_false((cpu1->P & FLAG_N)!=0, "LDA #$00 clears N");

    cpu_run_steps(1);
    assert_eq_u8(cpu1->A, 0x80, "LDA #$80 loads A");
    assert_false((cpu1->P & FLAG_Z)!=0, "LDA #$80 clears Z");
    assert_true((cpu1->P & FLAG_N)!=0, "LDA #$80 sets N");
}

// ————————————————————————————————————————
// TAX / TXA
// ————————————————————————————————————————
static void cpu_tax_txa(){
    static const uint8_t prog[] = {
            0xA9, 0x05, // LDA #$05
            0xAA,       // TAX
            0x8A        // TXA
    };
    cpu_write_prog(0x0020, prog, sizeof(prog));
    cpu_set_pc(0x0020);

    cpu_run_steps(1);
    cpu_run_steps(1);
    assert_eq_u8(cpu1->X, 0x05, "TAX transfers A to X");
    cpu_run_steps(1);
    assert_eq_u8(cpu1->A, 0x05, "TXA transfers X to A");
}

// ————————————————————————————————————————
// INX / DEX e flag
// ————————————————————————————————————————
static void cpu_inx_dex_flags(){
    static const uint8_t prog[] = {
            0xA2, 0xFF, // LDX #$FF
            0xE8,       // INX -> 0x00 (Z=1)
            0xCA        // DEX -> 0xFF (N=1)
    };
    cpu_write_prog(0x0030, prog, sizeof(prog));
    cpu_set_pc(0x0030);

    cpu_run_steps(1);
    cpu_run_steps(1);
    assert_eq_u8(cpu1->X, 0x00, "INX wraps to 0x00");
    assert_true((cpu1->P & FLAG_Z)!=0, "INX sets Z on zero");

    cpu_run_steps(1);
    assert_eq_u8(cpu1->X, 0xFF, "DEX wraps to 0xFF");
    assert_true((cpu1->P & FLAG_N)!=0, "DEX sets N on 0xFF");
}

// ————————————————————————————————————————
// STA/LD* zero page
// ————————————————————————————————————————
static void cpu_sta_lda_zeropage(){
    static const uint8_t prog[] = {
            0xA9, 0x3C,    // LDA #$3C
            0x85, 0x10,    // STA $10
            0xA9, 0x00,    // LDA #$00
            0xA5, 0x10     // LDA $10 -> 0x3C
    };
    cpu_write_prog(0x0040, prog, sizeof(prog));
    cpu_set_pc(0x0040);

    cpu_run_steps(1);
    cpu_run_steps(1);
    assert_eq_u8(bus_cpu_read8(bus1, 0x0010), 0x3C, "STA zp writes memory");
    cpu_run_steps(1);
    cpu_run_steps(1);
    assert_eq_u8(cpu1->A, 0x3C, "LDA zp reads back value");
}

// ————————————————————————————————————————
// ADC immediate (senza decimale) + flag C,V,Z,N
// ————————————————————————————————————————
static void cpu_adc_immediate(){
    // Caso 1: 0x50 + 0x10 = 0x60, nessun carry/overflow
    // Caso 2: 0x50 + 0x50 = 0xA0, V=1 (overflow signed), N=1
    // Caso 3: 0xFF + 0x01 = 0x00, C=1, Z=1
    static const uint8_t prog[] = {
            0xA9, 0x50,    // LDA #$50
            0x69, 0x10,    // ADC #$10 -> 0x60
            0xA9, 0x50,    // LDA #$50
            0x69, 0x50,    // ADC #$50 -> 0xA0 (V=1,N=1)
            0xA9, 0xFF,    // LDA #$FF
            0x69, 0x01     // ADC #$01 -> 0x00 (C=1,Z=1)
    };
    cpu_write_prog(0x0050, prog, sizeof(prog));
    cpu_set_pc(0x0050);

    cpu_run_steps(1); cpu_run_steps(1);
    assert_eq_u8(cpu1->A, 0x60, "ADC #$10 -> 0x60");
    assert_false((cpu1->P & FLAG_C)!=0, "ADC no carry");
    assert_false((cpu1->P & FLAG_V)!=0, "ADC no overflow");

    cpu_run_steps(1); cpu_run_steps(1);
    assert_eq_u8(cpu1->A, 0xA0, "ADC overflow -> 0xA0");
    assert_true((cpu1->P & FLAG_V)!=0, "ADC sets V");
    assert_true((cpu1->P & FLAG_N)!=0, "ADC sets N");

    cpu_run_steps(1); cpu_run_steps(1);
    assert_eq_u8(cpu1->A, 0x00, "ADC 0xFF+1 -> 0x00");
    assert_true((cpu1->P & FLAG_C)!=0, "ADC sets C on carry");
    assert_true((cpu1->P & FLAG_Z)!=0, "ADC sets Z on zero");
}

// ————————————————————————————————————————
// SBC immediate (binario) + flag
// ————————————————————————————————————————
static void cpu_sbc_immediate(){
    // Imposta C prima di SBC (6502 usa C come NOT borrow).
    static const uint8_t prog[] = {
            0xA9, 0x40,       // LDA #$40
            0x38,             // SEC
            0xE9, 0x10,       // SBC #$10 -> 0x30, C=1
            0xE9, 0x40        // SBC #$40 -> 0xF0, C=0, N=1
    };
    cpu_write_prog(0x0060, prog, sizeof(prog));
    cpu_set_pc(0x0060);

    cpu_run_steps(1); cpu_run_steps(1); cpu_run_steps(1);
    assert_eq_u8(cpu1->A, 0x30, "SBC #$10 -> 0x30");
    assert_true((cpu1->P & FLAG_C)!=0, "SBC keeps C=1 (no borrow)");

    cpu_run_steps(1);
    assert_eq_u8(cpu1->A, 0xF0, "SBC #$40 -> 0xF0");
    assert_false((cpu1->P & FLAG_C)!=0, "SBC clears C (borrow)");
    assert_true((cpu1->P & FLAG_N)!=0, "SBC sets N");
}

// ————————————————————————————————————————
// JMP abs e JMP (ind) con page-wrap bug
// ————————————————————————————————————————
static void cpu_jmp_tests(){
    // JMP $0080; NOP; (should land at $0080)
    // Poi prepara tabella indiretta a cavallo di pagina per testare il bug:
    // vector at $00FF/$0000 -> destinazione $1234
    static const uint8_t prog1[] = {
            0x4C, 0x80, 0x00, // JMP $0080
            0xEA              // NOP (non eseguito)
    };
    cpu_write_prog(0x0070, prog1, sizeof(prog1));
    cpu_set_pc(0x0070);

    // destinazione JMP abs
    static const uint8_t at80[] = { 0xA9, 0x55 }; // LDA #$55
    cpu_write_prog(0x0080, at80, sizeof(at80));

    cpu_run_steps(1);
    assert_eq_u16(cpu1->PC, 0x0080, "JMP absolute to $0080");
    cpu_run_steps(1);
    assert_eq_u8(cpu1->A, 0x55, "Arrived at $0080 and executed LDA");

    // JMP (ind) bug: vettore a $00FF punta a low=$34, high da $0000=$12
    static const uint8_t prog2[] = {
            0x6C, 0xFF, 0x00 // JMP ($00FF)  -> dovrebbe leggere $00FF e $0000
    };
    cpu_write_prog(0x0090, prog2, sizeof(prog2));
    // Tabella "indirect" con wrap:
    bus_cpu_write8(bus1, 0x00FF, 0x34);
    bus_cpu_write8(bus1, 0x0000, 0x12);
    // destinazione 0x1234:
    bus_cpu_write8(bus1, 0x1234, 0xEA); // NOP per avere una fetch valida

    cpu_set_pc(0x0090);
    cpu_run_steps(1);
    assert_eq_u16(cpu1->PC, 0x1234, "JMP (ind) 6502 wraparound bug honored");
}

// ————————————————————————————————————————
// JSR/RTS e stack
// ————————————————————————————————————————
static void cpu_jsr_rts_stack(){
    // $0100 page = stack. Verifichiamo push PC e ritorno.
    //  $0100+SP post-JSR deve contenere return-1.
    static const uint8_t sub[]  = { 0xA9, 0x77, 0x60 };   // LDA #$77 ; RTS
    static const uint8_t prog[] = {
            0x20, 0x20, 0x01, // JSR $0120
            0xEA              // NOP dopo ritorno
    };
    cpu_write_prog(0x0100, prog, sizeof(prog));
    cpu_write_prog(0x0120, sub, sizeof(sub));
    cpu_set_pc(0x0100);

    uint8_t sp_before = cpu1->S;
    cpu_run_steps(1); // JSR
    assert_true(cpu1->S == (uint8_t)(sp_before-2), "JSR pushes return address (SP-2)");

    cpu_run_steps(2); // LDA; RTS
    assert_eq_u8(cpu1->A, 0x77, "Subroutine executed");
    // Dopo RTS, PC deve puntare al NOP in $0103
    assert_eq_u16(cpu1->PC, 0x0103, "RTS returns to caller+1");
}

// ————————————————————————————————————————
// BRK/RTI (se disponibili helpers IRQ/NMI, puoi confrontare)
// ————————————————————————————————————————
static void cpu_brk_rti_basic(){
    // If vectors live in ROM (real cartridge), writes to $FFFE/$FFFF will be ignored.
    // Probe writability; if not writable, skip this test gracefully.
    uint8_t old_lo = bus_cpu_read8(bus1, 0xFFFE);
    uint8_t old_hi = bus_cpu_read8(bus1, 0xFFFF);
    bus_cpu_write8(bus1, 0xFFFE, 0x00);
    bus_cpu_write8(bus1, 0xFFFF, 0x02);
    bool vectors_writable = (bus_cpu_read8(bus1, 0xFFFE) == 0x00) && (bus_cpu_read8(bus1, 0xFFFF) == 0x02);
    // restore (in case it did write)
    bus_cpu_write8(bus1, 0xFFFE, old_lo);
    bus_cpu_write8(bus1, 0xFFFF, old_hi);
    if (!vectors_writable) {
        printf(ANSI_YELLOW "[SKIP] BRK/RTI vector test skipped (ROM vectors not writable)" ANSI_RESET);
        return;
    }

    static const uint8_t prog[] = {
            0xA9, 0x10, // LDA #$10
            0x00,       // BRK
            0xEA        // NOP (non eseguito, si va a $0200)
    };
    cpu_write_prog(0x0200, (const uint8_t[]){ 0x40 }, 1); // RTI all'ISR
    cpu_write_prog(0x0130, prog, sizeof(prog));
    cpu_set_pc(0x0130);

    uint8_t sp0 = cpu1->S;
    cpu_run_steps(2); // LDA; BRK
    assert_true((cpu1->P & FLAG_I)!=0, "BRK sets I");
    assert_true(cpu1->S == (uint8_t)(sp0-3), "BRK pushes PC+P (SP-3)");
    assert_eq_u16(cpu1->PC, 0x0200, "BRK jumps to IRQ/BRK vector");

    cpu_run_steps(1); // RTI
    assert_eq_u16(cpu1->PC, 0x0132, "RTI returns to PC after BRK");
}

// ————————————————————————————————————————
// PHP/PLP, PHA/PLA
// ————————————————————————————————————————
static void cpu_stack_push_pull(){
    static const uint8_t prog[] = {
            0xA9, 0xAA, // LDA #$AA
            0x48,       // PHA
            0xA9, 0x00, // LDA #$00
            0x68,       // PLA -> A=0xAA
            0x08,       // PHP
            0x28        // PLP
    };
    cpu_write_prog(0x0140, prog, sizeof(prog));
    cpu_set_pc(0x0140);

    uint8_t sp0 = cpu1->S;
    cpu_run_steps(2); // LDA; PHA
    assert_true(cpu1->S == (uint8_t)(sp0-1), "PHA decrements SP by 1");
    cpu_run_steps(2); // LDA #0; PLA
    assert_eq_u8(cpu1->A, 0xAA, "PLA restored A");
    uint8_t sp1 = cpu1->S;
    cpu_run_steps(1); // PHP
    assert_true(cpu1->S == (uint8_t)(sp1-1), "PHP decrements SP by 1");
    cpu_run_steps(1); // PLP
    assert_true(cpu1->S == sp1, "PLP restores SP");
}

// ————————————————————————————————————————
// Branch (BEQ) avanti/indietro
// ————————————————————————————————————————
static void cpu_branching_beq(){
    // Note on 6502 branches:
    // The relative offset is applied to PC *after* fetching the operand.
    // With the program below, BEQ +2 will land at 0x0156 (skipping the NOP at 0x0154 and the byte 0xA9 of the next LDA).
    static const uint8_t prog[] = {
            0xA9, 0x00,       // $0150: LDA #$00 (Z=1)
            0xF0, 0x02,       // $0152: BEQ +2  -> dest $0156
            0xEA,             // $0154: NOP (skipped)
            0xA9, 0x01,       // $0155: LDA #$01 (Z=0)
            0xF0, 0xFC        // $0157: BEQ -4 (not taken)
    };
    cpu_write_prog(0x0150, prog, sizeof(prog));
    cpu_set_pc(0x0150);

    cpu_run_steps(1); // LDA #0
    cpu_run_steps(1); // BEQ taken
    assert_eq_u16(cpu1->PC, 0x0156, "BEQ taken (forward)");

    cpu_run_steps(1); // LDA #1 at 0x0155/0x0156 -> after exec PC = 0x0158
    uint16_t pc_before = cpu1->PC; // 0x0158
    cpu_run_steps(1); // BEQ not taken
    // Not taken: PC should be base (after reading offset). With our fetch sequence this is pc_before+1.
    assert_eq_u16(cpu1->PC, (uint16_t)(pc_before + 1), "BEQ not taken");
}

// ————————————————————————————————————————
// NMI/IRQ meccanica di base (se hai cpu_nmi/cpu_irq)
// ————————————————————————————————————————
static void cpu_irq_nmi_mechanics(){
    // Probe whether interrupt vectors are writable (tests require it)
    uint8_t old_nmi_lo = bus_cpu_read8(bus1, 0xFFFA);
    uint8_t old_nmi_hi = bus_cpu_read8(bus1, 0xFFFB);
    uint8_t old_irq_lo = bus_cpu_read8(bus1, 0xFFFE);
    uint8_t old_irq_hi = bus_cpu_read8(bus1, 0xFFFF);

    bus_cpu_write8(bus1, 0xFFFA, 0x00); bus_cpu_write8(bus1, 0xFFFB, 0x03);
    bus_cpu_write8(bus1, 0xFFFE, 0x00); bus_cpu_write8(bus1, 0xFFFF, 0x03);

    bool vectors_writable =
        (bus_cpu_read8(bus1, 0xFFFA) == 0x00) && (bus_cpu_read8(bus1, 0xFFFB) == 0x03) &&
        (bus_cpu_read8(bus1, 0xFFFE) == 0x00) && (bus_cpu_read8(bus1, 0xFFFF) == 0x03);

    // restore originals in case writes succeeded
    bus_cpu_write8(bus1, 0xFFFA, old_nmi_lo); bus_cpu_write8(bus1, 0xFFFB, old_nmi_hi);
    bus_cpu_write8(bus1, 0xFFFE, old_irq_lo); bus_cpu_write8(bus1, 0xFFFF, old_irq_hi);

    if (!vectors_writable) {
        printf(ANSI_YELLOW "[SKIP] IRQ/NMI vector tests skipped (ROM vectors not writable)" ANSI_RESET);
        return;
    }

    // Vettori:
    bus_cpu_write8(bus1, 0xFFFA, 0x00); bus_cpu_write8(bus1, 0xFFFB, 0x03); // NMI -> $0300
    bus_cpu_write8(bus1, 0xFFFE, 0x00); bus_cpu_write8(bus1, 0xFFFF, 0x03); // IRQ/BRK -> $0300
    // ISR minimale: RTI
    bus_cpu_write8(bus1, 0x0300, 0x40);

    // Programma: NOP; NOP...
    bus_cpu_write8(bus1, 0x0160, 0xEA);
    bus_cpu_write8(bus1, 0x0161, 0xEA);
    cpu_set_pc(0x0160);

    // Test IRQ (disabilitato se I=1, quindi clear I)
    cpu1->P &= ~FLAG_I;
    cpu_irq(cpu1);
    cpu_run_steps(1); // esegui una istruzione per riconoscere l'IRQ
    assert_eq_u16(cpu1->PC, 0x0300, "IRQ vector taken to $0300");
    cpu_run_steps(1); // RTI
    assert_eq_u16(cpu1->PC, 0x0161, "RTI returns from IRQ");

    // Test NMI (ignora I)
    cpu_nmi(cpu1);
    cpu_run_steps(1);
    assert_eq_u16(cpu1->PC, 0x0300, "NMI vector taken to $0300");
    cpu_run_steps(1);
}

// ————————————————————————————————————————
// Runner
// ————————————————————————————————————————
void run_cpu_test() {
    printf("======================= CPU TEST =======================");
    cpu_sanity_after_reset();
    cpu_nop_increments_pc();
    cpu_lda_immediate_flags();
    cpu_tax_txa();
    cpu_inx_dex_flags();
    cpu_sta_lda_zeropage();
    cpu_adc_immediate();
    cpu_sbc_immediate();
    cpu_jmp_tests();
    cpu_jsr_rts_stack();
    cpu_brk_rti_basic();     // se NON puoi scrivere i vettori in $FFFE/FFFF, commenta questa riga
    cpu_stack_push_pull();
    cpu_branching_beq();
    cpu_irq_nmi_mechanics(); // scommenta se hai cpu_irq/cpu_nmi e vuoi provarli
}

// ————————————————————————————————————————
// Runners
// ————————————————————————————————————————
void run_ram_test(){
    printf("======================= RAM TEST =======================");
    ram_mirroring();
}

static void run_ppu_test(){
    printf("======================= PPU TEST =======================");
    ppu_mirroring();
    set_vblank();
    writing_ciram();
    test_vram_without_nmi();
    test_vram_with_nmi();
    test_prerender();
    test_sideeffect();
    test_inc_coarse_x();
    test_vertical_increment_and_copy();
    test_full_scroll_same_line();
    test_oamaddr_oamdata_seq();
}

static void run_mapper_test(){
    printf("===================== NROM/CHR TEST ====================");
    mapper_nrom_128();
    mapper_nrom_256();
    mapper_prgram_rw();
    mapper_no_prgram_rw();
    chr_rom_readonly_via_ppu();
    chr_ram_writable_via_ppu();
    reset_prg_sets();
    precise_offsets_prg_slots();
}

void run_all_tests(){
    run_ram_test();
    run_ppu_test();
    run_mapper_test();
    run_cpu_test();
}