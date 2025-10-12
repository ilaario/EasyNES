//
// Created by Dario Bonfiglio on 10/11/25.
//

#include "headers/cpu.h"

// ====== FLAGS ======
const uint8_t FLAG_C = 0x01;
const uint8_t FLAG_Z = 0x02;
const uint8_t FLAG_I = 0x04;
const uint8_t FLAG_D = 0x08;
const uint8_t FLAG_B = 0x10;   // usato solo su push P da BRK
const uint8_t FLAG_U = 0x20;   // bit 5 sempre = 1 quando pushi P
const uint8_t FLAG_V = 0x40;
const uint8_t FLAG_N = 0x80;


static inline uint8_t cpu_read8(cpu c, uint16_t addr){
    return bus_cpu_read8(c->sysbus, addr);
}

static inline void cpu_write8(cpu c, uint16_t addr, uint8_t v){
    bus_cpu_write8(c->sysbus, addr, v);
}


static inline uint8_t pull8(cpu c){
    return cpu_read8(c, 0x0100 | (++c->S));  // 6502: stack cresce verso il basso
}

static inline void push8(cpu c, uint8_t v){
    cpu_write8(c, 0x0100 | (c->S--), v);
}

static inline uint16_t pull16(cpu c){
    uint8_t lo=pull8(c), hi=pull8(c);
    return (uint16_t)hi<<8 | lo;
}

static inline void push16(cpu c, uint16_t v){
    // 6502 stack order for 16-bit values: push high byte, then low byte
    push8(c, (uint8_t)(v >> 8));
    push8(c, (uint8_t)(v & 0xFF));
}

// ========================
// Fetch & indirizzi
// ========================

static inline uint8_t fetch_imm8(cpu c){
    uint8_t v = cpu_read8(c, c->PC);
    c->PC++;
    return v;
}

static inline uint16_t fetch_imm16(cpu c){
    uint8_t lo=fetch_imm8(c), hi=fetch_imm8(c);
    return (uint16_t)hi<<8 | lo;
}

static inline uint8_t page_crossed(uint16_t a, uint16_t b){
    return (uint8_t)((a & 0xFF00)!=(b & 0xFF00));
}

// Zero page
static inline uint16_t addr_zp(cpu c){
    return (uint16_t)fetch_imm8(c);
}

static inline uint16_t addr_zp_x(cpu c){
    return (uint8_t)(fetch_imm8(c) + c->X);
}

static inline uint16_t addr_zp_y(cpu c){
    return (uint8_t)(fetch_imm8(c) + c->Y);
}

// Assoluti
static inline uint16_t addr_abs(cpu c){
    return fetch_imm16(c);
}

static inline uint16_t addr_abs_x(cpu c, bool* crossed){
    uint16_t base=fetch_imm16(c);
    uint16_t eff=base + c->X;
    if(crossed) *crossed = page_crossed(base,eff);
    return eff;
}

static inline uint16_t addr_abs_y(cpu c, bool* crossed){
    uint16_t base=fetch_imm16(c);
    uint16_t eff=base + c->Y;
    if(crossed) *crossed = page_crossed(base,eff);
    return eff;
}

static inline uint16_t addr_ind_x(cpu c){
    uint8_t zp = fetch_imm8(c);
    uint8_t prt = (uint8_t)(zp + c -> X);
    uint8_t lo = cpu_read8(c, (uint16_t)prt);
    uint8_t hi = cpu_read8(c, (uint16_t)((prt + 1) & 0xFF));
    return (uint16_t)((uint16_t)hi << 8) | (uint16_t)lo;
}

static inline uint16_t addr_ind_y(cpu c, bool* crossed){
    uint8_t zp = fetch_imm8(c);
    uint8_t lo = cpu_read8(c, (uint16_t)zp);
    uint8_t hi = cpu_read8(c, (uint16_t)((zp + 1) & 0xFF));
    uint16_t base = (uint16_t)((uint16_t)hi << 8) | (uint16_t)lo;
    uint16_t eff = (uint16_t)(base + c -> Y);
    if(crossed != NULL) *crossed = page_crossed(base, eff);
    return eff;
}

static inline uint16_t jmp_ind(cpu c){
    uint16_t prt = fetch_imm16(c);
    uint8_t lo = cpu_read8(c, prt);
    uint16_t hi_addr = (uint16_t)((prt & 0xFF00) | ((prt + 1) & 0x00FF));
    uint8_t hi = cpu_read8(c, hi_addr);
    return (uint16_t)((uint16_t)hi << 8) | (uint16_t)lo;
}


static inline void set_flag(cpu c, uint8_t mask, bool on){
    if(on) c -> P |= mask;
    else   c -> P &= ~mask;
}

static inline void set_ZN(cpu c, uint8_t v){
    set_flag(c, FLAG_Z, (v == 0));
    set_flag(c, FLAG_N, ((v & 0x80) != 0));
}

static inline uint8_t adc_core(cpu c, uint8_t m){
    uint8_t a = c -> A;
    uint8_t carry_in = (uint8_t)((c -> P & FLAG_C) != 0 ? 1 : 0);
    uint16_t sum16 = (uint16_t)a + (uint16_t)m + (uint16_t)carry_in;
    uint8_t res = (uint8_t)(sum16 & 0xFF);

    set_flag(c, FLAG_C,(sum16 > 0xFF));

    bool v_on = (((~(a ^ m)) & (a ^ res) & 0x80) != 0);
    set_flag(c, FLAG_V, v_on);
    return res;
}

static inline uint8_t sbc_core(cpu c, uint8_t m){
    uint8_t inv = (uint8_t)(m ^ 0xFF);
    return adc_core(c, inv);
}

static inline void do_cpm(cpu c, uint8_t a, uint8_t b){
    uint16_t diff = (uint16_t)a - (uint16_t)b;
    uint8_t res = (uint8_t)(diff & 0xFF);

    set_flag(c, FLAG_C, (a >= b));
    set_ZN(c, res);
}

bool get_flag(cpu c, uint8_t mask){
    return (c -> P & mask) != 0;
}

// ========================
// LDA
// ========================
int op_LDA_imm(cpu c){
    uint8_t m = fetch_imm8(c);
    c -> A = m;
    set_ZN(c, c -> A);
    return 2;
}

int op_LDA_zp(cpu c){
    uint8_t addr = addr_zp(c);
    c -> A = cpu_read8(c, addr);
    set_ZN(c, c -> A);
    return 3;
}

int op_LDA_abs(cpu c){
    uint16_t addr = addr_abs(c);
    c -> A = cpu_read8(c, addr);
    set_ZN(c, c -> A);
    return 4;
}

int op_LDA_abs_x(cpu c){
    bool crossed = false;
    uint16_t addr = addr_abs_x(c, &crossed);
    c -> A = cpu_read8(c, addr);
    set_ZN(c, c -> A);
    return (crossed ? 5 : 4);
}

int op_LDA_abs_y(cpu c){
    bool crossed = false;
    uint16_t addr = addr_abs_y(c, &crossed);
    c -> A = cpu_read8(c, addr);
    set_ZN(c, c -> A);
    return (crossed ? 5 : 4);
}

int op_LDA_ind_x(cpu c){
    uint16_t addr = addr_ind_x(c);
    c -> A = cpu_read8(c, addr);
    set_ZN(c, c -> A);
    return 6;
}

int op_LDA_ind_y(cpu c){
    bool crossed = false;
    uint16_t addr = addr_ind_y(c, &crossed);
    c -> A = cpu_read8(c, addr);
    set_ZN(c, c -> A);
    return (crossed ? 6 : 5);
}

int op_LDX_imm(cpu c){
    uint8_t m = fetch_imm8(c);
    c -> X = m;
    set_ZN(c, c -> X);
    return 2;
}

int op_LDY_imm(cpu c){
    uint8_t m = fetch_imm8(c);
    c -> Y = m;
    set_ZN(c, c -> Y);
    return 2;
}

int op_STA_zp(cpu c){
    uint16_t addr = addr_zp(c);
    cpu_write8(c, addr, c -> A);
    return 3;
}

int op_STA_abs(cpu c){
    uint16_t addr = addr_abs(c);
    cpu_write8(c, addr, c -> A);
    return 4;
}

int op_STA_abs_x(cpu c){
    bool crossed = false;
    uint16_t addr = addr_abs_x(c, &crossed);
    cpu_write8(c, addr, c -> A);
    return 5;
}

int op_STA_abs_y(cpu c){
    bool crossed = false;
    uint16_t addr = addr_abs_y(c, &crossed);
    cpu_write8(c, addr, c -> A);
    return 5;
}

int op_STA_ind_x(cpu c){
    uint16_t addr = addr_ind_x(c);
    cpu_write8(c, addr, c -> A);
    return 6;
}

int op_STA_ind_y(cpu c){
    bool crossed = false;
    uint16_t addr = addr_ind_y(c, &crossed);
    cpu_write8(c, addr, c -> A);
    return 6;
}

// AND — tutte le varianti

int op_AND_imm(cpu c) {
    uint8_t m = fetch_imm8(c);
    c->A &= m;
    set_ZN(c, c->A);
    return 2;
}

int op_AND_zp(cpu c) {
    uint16_t a = addr_zp(c);
    uint8_t m = cpu_read8(c, a);
    c->A &= m;
    set_ZN(c, c->A);
    return 3;
}

int op_AND_abs(cpu c) {
    uint16_t a = addr_abs(c);
    uint8_t m = cpu_read8(c, a);
    c->A &= m;
    set_ZN(c, c->A);
    return 4;
}

int op_AND_abs_x(cpu c) {
    bool crossed = false;
    uint16_t a = addr_abs_x(c, &crossed);
    uint8_t m = cpu_read8(c, a);
    c->A &= m;
    set_ZN(c, c->A);
    return crossed ? 5 : 4;
}

int op_AND_abs_y(cpu c) {
    bool crossed = false;
    uint16_t a = addr_abs_y(c, &crossed);
    uint8_t m = cpu_read8(c, a);
    c->A &= m;
    set_ZN(c, c->A);
    return crossed ? 5 : 4;
}

int op_AND_ind_x(cpu c) {
    uint16_t a = addr_ind_x(c);
    uint8_t m = cpu_read8(c, a);
    c->A &= m;
    set_ZN(c, c->A);
    return 6;
}

int op_AND_ind_y(cpu c) {
    bool crossed = false;
    uint16_t a = addr_ind_y(c, &crossed);
    uint8_t m = cpu_read8(c, a);
    c->A &= m;
    set_ZN(c, c->A);
    return crossed ? 6 : 5;
}


// ORA — tutte le varianti

int op_ORA_imm(cpu c) {
    uint8_t m = fetch_imm8(c);
    c->A |= m;
    set_ZN(c, c->A);
    return 2;
}

int op_ORA_zp(cpu c) {
    uint16_t a = addr_zp(c);
    uint8_t m = cpu_read8(c, a);
    c->A |= m;
    set_ZN(c, c->A);
    return 3;
}

int op_ORA_abs(cpu c) {
    uint16_t a = addr_abs(c);
    uint8_t m = cpu_read8(c, a);
    c->A |= m;
    set_ZN(c, c->A);
    return 4;
}

int op_ORA_abs_x(cpu c) {
    bool crossed = false;
    uint16_t a = addr_abs_x(c, &crossed);
    uint8_t m = cpu_read8(c, a);
    c->A |= m;
    set_ZN(c, c->A);
    return crossed ? 5 : 4;
}

int op_ORA_abs_y(cpu c) {
    bool crossed = false;
    uint16_t a = addr_abs_y(c, &crossed);
    uint8_t m = cpu_read8(c, a);
    c->A |= m;
    set_ZN(c, c->A);
    return crossed ? 5 : 4;
}

int op_ORA_ind_x(cpu c) {
    uint16_t a = addr_ind_x(c);
    uint8_t m = cpu_read8(c, a);
    c->A |= m;
    set_ZN(c, c->A);
    return 6;
}

int op_ORA_ind_y(cpu c) {
    bool crossed = false;
    uint16_t a = addr_ind_y(c, &crossed);
    uint8_t m = cpu_read8(c, a);
    c->A |= m;
    set_ZN(c, c->A);
    return crossed ? 6 : 5;
}


// EOR — tutte le varianti

int op_EOR_imm(cpu c) {
    uint8_t m = fetch_imm8(c);
    c->A ^= m;
    set_ZN(c, c->A);
    return 2;
}

int op_EOR_zp(cpu c) {
    uint16_t a = addr_zp(c);
    uint8_t m = cpu_read8(c, a);
    c->A ^= m;
    set_ZN(c, c->A);
    return 3;
}

int op_EOR_abs(cpu c) {
    uint16_t a = addr_abs(c);
    uint8_t m = cpu_read8(c, a);
    c->A ^= m;
    set_ZN(c, c->A);
    return 4;
}

int op_EOR_abs_x(cpu c) {
    bool crossed = false;
    uint16_t a = addr_abs_x(c, &crossed);
    uint8_t m = cpu_read8(c, a);
    c->A ^= m;
    set_ZN(c, c->A);
    return crossed ? 5 : 4;
}

int op_EOR_abs_y(cpu c) {
    bool crossed = false;
    uint16_t a = addr_abs_y(c, &crossed);
    uint8_t m = cpu_read8(c, a);
    c->A ^= m;
    set_ZN(c, c->A);
    return crossed ? 5 : 4;
}

int op_EOR_ind_x(cpu c) {
    uint16_t a = addr_ind_x(c);
    uint8_t m = cpu_read8(c, a);
    c->A ^= m;
    set_ZN(c, c->A);
    return 6;
}

int op_EOR_ind_y(cpu c) {
    bool crossed = false;
    uint16_t a = addr_ind_y(c, &crossed);
    uint8_t m = cpu_read8(c, a);
    c->A ^= m;
    set_ZN(c, c->A);
    return crossed ? 6 : 5;
}


// ADC — varianti richieste

int op_ADC_imm(cpu c) {
    uint8_t m = fetch_imm8(c);
    c->A = adc_core(c, m);     // adc_core deve settare C e V
    set_ZN(c, c->A);
    return 2;
}

int op_ADC_abs_x(cpu c) {
    bool crossed = false;
    uint16_t a = addr_abs_x(c, &crossed);
    uint8_t m = cpu_read8(c, a);
    c->A = adc_core(c, m);
    set_ZN(c, c->A);
    return crossed ? 5 : 4;
}

int op_ADC_ind_y(cpu c) {
    bool crossed = false;
    uint16_t a = addr_ind_y(c, &crossed);
    uint8_t m = cpu_read8(c, a);
    c->A = adc_core(c, m);
    set_ZN(c, c->A);
    return crossed ? 6 : 5;
}


// SBC — variante richiesta

int op_SBC_imm(cpu c) {
    uint8_t m = fetch_imm8(c);
    c->A = sbc_core(c, m);     // sbc_core deve settare C e V
    set_ZN(c, c->A);
    return 2;
}

// =====================
// CMP / CPX / CPY (imm)
// =====================

int op_CMP_imm(cpu c) {
    uint8_t m = fetch_imm8(c);
    do_cpm(c, c->A, m);
    return 2;
}

int op_CPX_imm(cpu c) {
    uint8_t m = fetch_imm8(c);
    do_cpm(c, c->X, m);
    return 2;
}

int op_CPY_imm(cpu c) {
    uint8_t m = fetch_imm8(c);
    do_cpm(c, c->Y, m);
    return 2;
}


// =====================
// INX/DEX/INY/DEY
// =====================

int op_INX(cpu c) {
    c->X = (uint8_t)(c->X + 1);
    set_ZN(c, c->X);
    return 2;
}

int op_DEX(cpu c) {
    c->X = (uint8_t)(c->X - 1);
    set_ZN(c, c->X);
    return 2;
}

int op_INY(cpu c) {
    c->Y = (uint8_t)(c->Y + 1);
    set_ZN(c, c->Y);
    return 2;
}

int op_DEY(cpu c) {
    c->Y = (uint8_t)(c->Y - 1);
    set_ZN(c, c->Y);
    return 2;
}


// =====================
// INC/DEC (zero page)
// =====================

int op_INC_zp(cpu c) {
    uint16_t a = addr_zp(c);
    uint8_t v = cpu_read8(c, a);
    v = (uint8_t)(v + 1);
    cpu_write8(c, a, v);
    set_ZN(c, v);
    return 5;
}

int op_DEC_zp(cpu c) {
    uint16_t a = addr_zp(c);
    uint8_t v = cpu_read8(c, a);
    v = (uint8_t)(v - 1);
    cpu_write8(c, a, v);
    set_ZN(c, v);
    return 5;
}


// =====================
// Shifts/Rotates (Accumulator)
// =====================

int op_ASL_A(cpu c) {
    uint8_t v = c->A;
    set_flag(c, FLAG_C, (v & 0x80) != 0);
    v <<= 1;
    c->A = v;
    set_ZN(c, v);
    return 2;
}

int op_LSR_A(cpu c) {
    uint8_t v = c->A;
    set_flag(c, FLAG_C, (v & 0x01) != 0);
    v >>= 1;
    c->A = v;
    set_ZN(c, v);
    return 2;
}

int op_ROL_A(cpu c) {
    uint8_t v = c->A;
    uint8_t oldC = (uint8_t)((c->P & FLAG_C) ? 1 : 0);
    set_flag(c, FLAG_C, (v & 0x80) != 0);
    v = (uint8_t)((v << 1) | oldC);
    c->A = v;
    set_ZN(c, v);
    return 2;
}

int op_ROR_A(cpu c) {
    uint8_t v = c->A;
    uint8_t oldC = (uint8_t)((c->P & FLAG_C) ? 1 : 0);
    set_flag(c, FLAG_C, (v & 0x01) != 0);
    v = (uint8_t)((v >> 1) | (oldC << 7));
    c->A = v;
    set_ZN(c, v);
    return 2;
}


// =====================
// Shifts/Rotates (zero page)
// =====================

int op_ASL_zp(cpu c) {
    uint16_t a = addr_zp(c);
    uint8_t v = cpu_read8(c, a);
    set_flag(c, FLAG_C, (v & 0x80) != 0);
    v <<= 1;
    cpu_write8(c, a, v);
    set_ZN(c, v);
    return 5;
}

int op_LSR_zp(cpu c) {
    uint16_t a = addr_zp(c);
    uint8_t v = cpu_read8(c, a);
    set_flag(c, FLAG_C, (v & 0x01) != 0);
    v >>= 1;
    cpu_write8(c, a, v);
    set_ZN(c, v);
    return 5;
}

int op_ROL_zp(cpu c) {
    uint16_t a = addr_zp(c);
    uint8_t v = cpu_read8(c, a);
    uint8_t oldC = (uint8_t)((c->P & FLAG_C) ? 1 : 0);
    set_flag(c, FLAG_C, (v & 0x80) != 0);
    v = (uint8_t)((v << 1) | oldC);
    cpu_write8(c, a, v);
    set_ZN(c, v);
    return 5;
}

int op_ROR_zp(cpu c) {
    uint16_t a = addr_zp(c);
    uint8_t v = cpu_read8(c, a);
    uint8_t oldC = (uint8_t)((c->P & FLAG_C) ? 1 : 0);
    set_flag(c, FLAG_C, (v & 0x01) != 0);
    v = (uint8_t)((v >> 1) | (oldC << 7));
    cpu_write8(c, a, v);
    set_ZN(c, v);
    return 5;
}


// =====================
// BIT (zero page)
// =====================

int op_BIT_zp(cpu c) {
    uint16_t a = addr_zp(c);
    uint8_t v = cpu_read8(c, a);
    set_flag(c, FLAG_Z, ( (c->A & v) == 0 ));
    set_flag(c, FLAG_V, (v & 0x40) != 0);
    set_flag(c, FLAG_N, (v & 0x80) != 0);
    return 3;
}


// =====================
// Jumps / Subroutines
// =====================

int op_JMP_abs(cpu c) {
    c->PC = addr_abs(c);
    return 3;
}

int op_JMP_ind(cpu c) {
    c->PC = jmp_ind(c);
    return 5;
}

int op_JSR(cpu c) {
    uint16_t target = addr_abs(c);
    uint16_t ret = (uint16_t)(c->PC - 1);
    push16(c, ret);
    c->PC = target;
    return 6;
}

int op_RTS(cpu c) {
    uint16_t ret = pull16(c);
    c->PC = (uint16_t)(ret + 1);
    return 6;
}


// =====================
// Branch helper + branch ops
// =====================

int branch(cpu c, bool take) {
    int8_t off = (int8_t)fetch_imm8(c);   // fetch signed offset; PC now points to next instruction (base)
    uint16_t base = c->PC;                // base = address after the branch instruction
    if (take) {
        uint16_t dst = (uint16_t)(base + off);
        int cycles = 3;                   // 2 + 1 for taken
        if (page_crossed(base, dst)) cycles += 1;
        c->PC = dst;
        return cycles;
    }
    return 2;                             // not taken: just 2 cycles, PC already at base
}

int op_BEQ(cpu c) { return branch(c, (c->P & FLAG_Z) != 0); }
int op_BNE(cpu c) { return branch(c, (c->P & FLAG_Z) == 0); }
int op_BCS(cpu c) { return branch(c, (c->P & FLAG_C) != 0); }
int op_BCC(cpu c) { return branch(c, (c->P & FLAG_C) == 0); }
int op_BMI(cpu c) { return branch(c, (c->P & FLAG_N) != 0); }
int op_BPL(cpu c) { return branch(c, (c->P & FLAG_N) == 0); }
int op_BVS(cpu c) { return branch(c, (c->P & FLAG_V) != 0); }
int op_BVC(cpu c) { return branch(c, (c->P & FLAG_V) == 0); }


// =====================
// Stack & Status
// =====================

int op_PHA(cpu c) {
    push8(c, c->A);
    return 3;
}

int op_PLA(cpu c) {
    c->A = pull8(c);
    set_ZN(c, c->A);
    return 4;
}

int op_PHP(cpu c) {
    // Quando si pusha P, bit U e B vanno a 1
    uint8_t P_to_push = (uint8_t)(c->P | FLAG_U | FLAG_B);
    push8(c, P_to_push);
    return 3;
}

int op_PLP(cpu c) {
    // Quando si poppa P, B=0 e U=1
    c->P = (uint8_t)((pull8(c) & ~FLAG_B) | FLAG_U);
    return 4;
}


// =====================
// Flag singole
// =====================

int op_SEC(cpu c) { set_flag(c, FLAG_C, true);  return 2; }
int op_CLC(cpu c) { set_flag(c, FLAG_C, false); return 2; }
int op_SEI(cpu c) { set_flag(c, FLAG_I, true);  return 2; }
int op_CLI(cpu c) { set_flag(c, FLAG_I, false); return 2; }
int op_CLV(cpu c) { set_flag(c, FLAG_V, false); return 2; }
int op_CLD(cpu c) { set_flag(c, FLAG_D, false); return 2; }
int op_SED(cpu c) { set_flag(c, FLAG_D, true);  return 2; }


// =====================
// BRK / RTI
// =====================

int op_BRK(cpu c) {
    // BRK consuma il byte successivo
    c->PC = (uint16_t)(c->PC + 1);

    // Push PC e P|B|U, set I
    push16(c, c->PC);
    push8(c, (uint8_t)(c->P | FLAG_B | FLAG_U));
    set_flag(c, FLAG_I, true);

    // Vettore IRQ/BRK
    uint8_t lo = cpu_read8(c, 0xFFFE);
    uint8_t hi = cpu_read8(c, 0xFFFF);
    c->PC = (uint16_t)(((uint16_t)hi << 8) | lo);
    return 7;
}

int op_RTI(cpu c) {
    // Pull P (B=0, U=1), poi PC
    c->P = (uint8_t)((pull8(c) & ~FLAG_B) | FLAG_U);
    c->PC = pull16(c);
    return 6;
}

// =====================
// Transfer & Stack
// =====================

int op_TAX(cpu c) { c->X = c->A; set_ZN(c, c->X); return 2; }
int op_TXA(cpu c) { c->A = c->X; set_ZN(c, c->A); return 2; }
int op_TAY(cpu c) { c->Y = c->A; set_ZN(c, c->Y); return 2; }
int op_TYA(cpu c) { c->A = c->Y; set_ZN(c, c->A); return 2; }
int op_TSX(cpu c) { c->X = c->S; set_ZN(c, c->X); return 2; }
int op_TXS(cpu c) { c->S = c->X; /* no flags */   return 2; }


// =====================
// LDX varianti
// =====================

int op_LDX_zp(cpu c) {
    uint16_t a = addr_zp(c);
    c->X = cpu_read8(c, a);
    set_ZN(c, c->X);
    return 3;
}

int op_LDX_zp_y(cpu c) {
    uint16_t a = addr_zp_y(c);     // wrap in ZP
    c->X = cpu_read8(c, a);
    set_ZN(c, c->X);
    return 4;
}

int op_LDX_abs(cpu c) {
    uint16_t a = addr_abs(c);
    c->X = cpu_read8(c, a);
    set_ZN(c, c->X);
    return 4;
}

int op_LDX_abs_y(cpu c) {
    bool crossed = false;
    uint16_t a = addr_abs_y(c, &crossed);
    c->X = cpu_read8(c, a);
    set_ZN(c, c->X);
    return crossed ? 5 : 4;
}


// =====================
// LDY varianti
// =====================

int op_LDY_zp(cpu c) {
    uint16_t a = addr_zp(c);
    c->Y = cpu_read8(c, a);
    set_ZN(c, c->Y);
    return 3;
}

int op_LDY_zp_x(cpu c) {
    uint16_t a = addr_zp_x(c);
    c->Y = cpu_read8(c, a);
    set_ZN(c, c->Y);
    return 4;
}

int op_LDY_abs(cpu c) {
    uint16_t a = addr_abs(c);
    c->Y = cpu_read8(c, a);
    set_ZN(c, c->Y);
    return 4;
}

int op_LDY_abs_x(cpu c) {
    bool crossed = false;
    uint16_t a = addr_abs_x(c, &crossed);
    c->Y = cpu_read8(c, a);
    set_ZN(c, c->Y);
    return crossed ? 5 : 4;
}


// =====================
// STA zp,X (0x95)
// =====================

int op_STA_zp_x(cpu c) {
    uint16_t a = addr_zp_x(c);
    cpu_write8(c, a, c->A);
    return 4;
}


// =====================
// INC / DEC (zp,X / abs / abs,X)
// =====================

int op_INC_zp_x(cpu c) {
    uint16_t a = addr_zp_x(c);
    uint8_t v = cpu_read8(c, a);
    v = (uint8_t)(v + 1);
    cpu_write8(c, a, v);
    set_ZN(c, v);
    return 6;
}

int op_INC_abs(cpu c) {
    uint16_t a = addr_abs(c);
    uint8_t v = cpu_read8(c, a);
    v = (uint8_t)(v + 1);
    cpu_write8(c, a, v);
    set_ZN(c, v);
    return 6;
}

int op_INC_abs_x(cpu c) {
    bool crossed = false; // ignorato per cicli R-M-W
    uint16_t a = addr_abs_x(c, &crossed);
    uint8_t v = cpu_read8(c, a);
    v = (uint8_t)(v + 1);
    cpu_write8(c, a, v);
    set_ZN(c, v);
    return 7;
}

int op_DEC_zp_x(cpu c) {
    uint16_t a = addr_zp_x(c);
    uint8_t v = cpu_read8(c, a);
    v = (uint8_t)(v - 1);
    cpu_write8(c, a, v);
    set_ZN(c, v);
    return 6;
}

int op_DEC_abs(cpu c) {
    uint16_t a = addr_abs(c);
    uint8_t v = cpu_read8(c, a);
    v = (uint8_t)(v - 1);
    cpu_write8(c, a, v);
    set_ZN(c, v);
    return 6;
}

int op_DEC_abs_x(cpu c) {
    bool crossed = false; // ignorato per cicli R-M-W
    uint16_t a = addr_abs_x(c, &crossed);
    uint8_t v = cpu_read8(c, a);
    v = (uint8_t)(v - 1);
    cpu_write8(c, a, v);
    set_ZN(c, v);
    return 7;
}


// =====================
// BIT abs (0x2C)
// =====================

int op_BIT_abs(cpu c) {
    uint16_t a = addr_abs(c);
    uint8_t v = cpu_read8(c, a);
    set_flag(c, FLAG_Z, ((c->A & v) == 0));
    set_flag(c, FLAG_V, (v & 0x40) != 0);
    set_flag(c, FLAG_N, (v & 0x80) != 0);
    return 4;
}


// =====================
// ADC varianti (zp, zp,X, abs, abs,Y, ind,X)
// =====================

int op_ADC_zp(cpu c) {
    uint16_t a = addr_zp(c);
    uint8_t m = cpu_read8(c, a);
    c->A = adc_core(c, m);
    set_ZN(c, c->A);
    return 3;
}

int op_ADC_zp_x(cpu c) {
    uint16_t a = addr_zp_x(c);
    uint8_t m = cpu_read8(c, a);
    c->A = adc_core(c, m);
    set_ZN(c, c->A);
    return 4;
}

int op_ADC_abs(cpu c) {
    uint16_t a = addr_abs(c);
    uint8_t m = cpu_read8(c, a);
    c->A = adc_core(c, m);
    set_ZN(c, c->A);
    return 4;
}

int op_ADC_abs_y(cpu c) {
    bool crossed = false;
    uint16_t a = addr_abs_y(c, &crossed);
    uint8_t m = cpu_read8(c, a);
    c->A = adc_core(c, m);
    set_ZN(c, c->A);
    return crossed ? 5 : 4;
}

int op_ADC_ind_x(cpu c) {
    uint16_t a = addr_ind_x(c);
    uint8_t m = cpu_read8(c, a);
    c->A = adc_core(c, m);
    set_ZN(c, c->A);
    return 6;
}


// =====================
// SBC varianti (zp, zp,X, abs, abs,Y, ind,X)
// =====================

int op_SBC_zp(cpu c) {
    uint16_t a = addr_zp(c);
    uint8_t m = cpu_read8(c, a);
    c->A = sbc_core(c, m);
    set_ZN(c, c->A);
    return 3;
}

int op_SBC_zp_x(cpu c) {
    uint16_t a = addr_zp_x(c);
    uint8_t m = cpu_read8(c, a);
    c->A = sbc_core(c, m);
    set_ZN(c, c->A);
    return 4;
}

int op_SBC_abs(cpu c) {
    uint16_t a = addr_abs(c);
    uint8_t m = cpu_read8(c, a);
    c->A = sbc_core(c, m);
    set_ZN(c, c->A);
    return 4;
}

int op_SBC_abs_y(cpu c) {
    bool crossed = false;
    uint16_t a = addr_abs_y(c, &crossed);
    uint8_t m = cpu_read8(c, a);
    c->A = sbc_core(c, m);
    set_ZN(c, c->A);
    return crossed ? 5 : 4;
}

int op_SBC_ind_x(cpu c) {
    uint16_t a = addr_ind_x(c);
    uint8_t m = cpu_read8(c, a);
    c->A = sbc_core(c, m);
    set_ZN(c, c->A);
    return 6;
}


// =====================
// CMP / CPX / CPY – zp/abs
// =====================

int op_CMP_zp(cpu c) {
    uint16_t a = addr_zp(c);
    uint8_t m = cpu_read8(c, a);
    do_cpm(c, c->A, m);
    return 3;
}

int op_CMP_abs(cpu c) {
    uint16_t a = addr_abs(c);
    uint8_t m = cpu_read8(c, a);
    do_cpm(c, c->A, m);
    return 4;
}

int op_CPX_zp(cpu c) {
    uint16_t a = addr_zp(c);
    uint8_t m = cpu_read8(c, a);
    do_cpm(c, c->X, m);
    return 3;
}

int op_CPX_abs(cpu c) {
    uint16_t a = addr_abs(c);
    uint8_t m = cpu_read8(c, a);
    do_cpm(c, c->X, m);
    return 4;
}

int op_CPY_zp(cpu c) {
    uint16_t a = addr_zp(c);
    uint8_t m = cpu_read8(c, a);
    do_cpm(c, c->Y, m);
    return 3;
}

int op_CPY_abs(cpu c) {
    uint16_t a = addr_abs(c);
    uint8_t m = cpu_read8(c, a);
    do_cpm(c, c->Y, m);
    return 4;
}


// =====================
// NOP standard (0xEA)
// =====================

int op_NOP(cpu c) {
    (void)c;
    return 2;
}

int op_SBC_abs_x(cpu c) {
    bool crossed = false;
    uint16_t a = addr_abs_x(c, &crossed);
    uint8_t m = cpu_read8(c, a);
    c->A = sbc_core(c, m);   // sbc_core setta C e V
    set_ZN(c, c->A);
    return crossed ? 5 : 4;  // +1 se page-cross
}

int op_SBC_ind_y(cpu c) {
    bool crossed = false;
    uint16_t a = addr_ind_y(c, &crossed);
    uint8_t m = cpu_read8(c, a);
    c->A = sbc_core(c, m);
    set_ZN(c, c->A);
    return crossed ? 6 : 5;  // +1 se page-cross
}

int op_LDA_zp_x(cpu c){
    uint16_t a = addr_zp_x(c);
    c->A = cpu_read8(c, a);
    set_ZN(c, c->A);
    return 4;
}

// STX
int op_STX_zp(cpu c){ uint16_t a=addr_zp(c);   cpu_write8(c,a,c->X); return 3; }
int op_STX_zp_y(cpu c){ uint16_t a=addr_zp_y(c); cpu_write8(c,a,c->X); return 4; }
int op_STX_abs(cpu c){ uint16_t a=addr_abs(c); cpu_write8(c,a,c->X); return 4; }

// STY
int op_STY_zp(cpu c){ uint16_t a=addr_zp(c);   cpu_write8(c,a,c->Y); return 3; }
int op_STY_zp_x(cpu c){ uint16_t a=addr_zp_x(c); cpu_write8(c,a,c->Y); return 4; }
int op_STY_abs(cpu c){ uint16_t a=addr_abs(c); cpu_write8(c,a,c->Y); return 4; }

// ASL
int op_ASL_zp_x(cpu c){
    uint16_t a=addr_zp_x(c); uint8_t v=cpu_read8(c,a);
    set_flag(c,FLAG_C,(v&0x80)!=0); v<<=1; cpu_write8(c,a,v); set_ZN(c,v); return 6;
}
int op_ASL_abs(cpu c){
    uint16_t a=addr_abs(c); uint8_t v=cpu_read8(c,a);
    set_flag(c,FLAG_C,(v&0x80)!=0); v<<=1; cpu_write8(c,a,v); set_ZN(c,v); return 6;
}
int op_ASL_abs_x(cpu c){
    bool crossed=false; uint16_t a=addr_abs_x(c,&crossed); uint8_t v=cpu_read8(c,a);
    set_flag(c,FLAG_C,(v&0x80)!=0); v<<=1; cpu_write8(c,a,v); set_ZN(c,v); return 7;
}

// LSR
int op_LSR_zp_x(cpu c){
    uint16_t a=addr_zp_x(c); uint8_t v=cpu_read8(c,a);
    set_flag(c,FLAG_C,(v&0x01)!=0); v>>=1; cpu_write8(c,a,v); set_ZN(c,v); return 6;
}
int op_LSR_abs(cpu c){
    uint16_t a=addr_abs(c); uint8_t v=cpu_read8(c,a);
    set_flag(c,FLAG_C,(v&0x01)!=0); v>>=1; cpu_write8(c,a,v); set_ZN(c,v); return 6;
}
int op_LSR_abs_x(cpu c){
    bool crossed=false; uint16_t a=addr_abs_x(c,&crossed); uint8_t v=cpu_read8(c,a);
    set_flag(c,FLAG_C,(v&0x01)!=0); v>>=1; cpu_write8(c,a,v); set_ZN(c,v); return 7;
}

// ROL
int op_ROL_zp_x(cpu c){
    uint16_t a=addr_zp_x(c); uint8_t v=cpu_read8(c,a); uint8_t oldC=(c->P&FLAG_C)?1:0;
    set_flag(c,FLAG_C,(v&0x80)!=0); v=(uint8_t)((v<<1)|oldC); cpu_write8(c,a,v); set_ZN(c,v); return 6;
}
int op_ROL_abs(cpu c){
    uint16_t a=addr_abs(c); uint8_t v=cpu_read8(c,a); uint8_t oldC=(c->P&FLAG_C)?1:0;
    set_flag(c,FLAG_C,(v&0x80)!=0); v=(uint8_t)((v<<1)|oldC); cpu_write8(c,a,v); set_ZN(c,v); return 6;
}
int op_ROL_abs_x(cpu c){
    bool crossed=false; uint16_t a=addr_abs_x(c,&crossed); uint8_t v=cpu_read8(c,a); uint8_t oldC=(c->P&FLAG_C)?1:0;
    set_flag(c,FLAG_C,(v&0x80)!=0); v=(uint8_t)((v<<1)|oldC); cpu_write8(c,a,v); set_ZN(c,v); return 7;
}

// ROR
int op_ROR_zp_x(cpu c){
    uint16_t a=addr_zp_x(c); uint8_t v=cpu_read8(c,a); uint8_t oldC=(c->P&FLAG_C)?1:0;
    set_flag(c,FLAG_C,(v&0x01)!=0); v=(uint8_t)((v>>1)|(oldC<<7)); cpu_write8(c,a,v); set_ZN(c,v); return 6;
}
int op_ROR_abs(cpu c){
    uint16_t a=addr_abs(c); uint8_t v=cpu_read8(c,a); uint8_t oldC=(c->P&FLAG_C)?1:0;
    set_flag(c,FLAG_C,(v&0x01)!=0); v=(uint8_t)((v>>1)|(oldC<<7)); cpu_write8(c,a,v); set_ZN(c,v); return 6;
}
int op_ROR_abs_x(cpu c){
    bool crossed=false; uint16_t a=addr_abs_x(c,&crossed); uint8_t v=cpu_read8(c,a); uint8_t oldC=(c->P&FLAG_C)?1:0;
    set_flag(c,FLAG_C,(v&0x01)!=0); v=(uint8_t)((v>>1)|(oldC<<7)); cpu_write8(c,a,v); set_ZN(c,v); return 7;
}

int op_CMP_zp_x(cpu c){
    uint16_t a=addr_zp_x(c); uint8_t m=cpu_read8(c,a); do_cpm(c,c->A,m); return 4;
}

int op_CMP_abs_x(cpu c){
    bool crossed=false; uint16_t a=addr_abs_x(c,&crossed); uint8_t m=cpu_read8(c,a);
    do_cpm(c,c->A,m); return crossed?5:4;
}

int op_CMP_abs_y(cpu c){
    bool crossed=false; uint16_t a=addr_abs_y(c,&crossed); uint8_t m=cpu_read8(c,a);
    do_cpm(c,c->A,m); return crossed?5:4;
}

int op_CMP_ind_x(cpu c){
    uint16_t a=addr_ind_x(c); uint8_t m=cpu_read8(c,a); do_cpm(c,c->A,m); return 6;
}

int op_CMP_ind_y(cpu c){
    bool crossed=false; uint16_t a=addr_ind_y(c,&crossed); uint8_t m=cpu_read8(c,a);
    do_cpm(c,c->A,m); return crossed?6:5;
}

int service_NMI(cpu c){
    push16(c, c -> PC);

    uint8_t P_to_push = (uint8_t)((c -> P & ~FLAG_B) | FLAG_U);
    push8(c, P_to_push);

    set_flag(c, FLAG_I, true);

    uint8_t lo = bus_cpu_read8(c -> sysbus, 0xFFFA);
    uint8_t hi = bus_cpu_read8(c -> sysbus, 0xFFFB);
    c -> PC = (uint16_t)(((uint16_t)hi << 8) | lo);

    return 7;
}

int service_IRQ(cpu c){
    push16(c, c -> PC);

    uint8_t P_to_push = (uint8_t)((c -> P & ~FLAG_B) | FLAG_U);
    push8(c, P_to_push);

    set_flag(c, FLAG_I, true);

    uint8_t lo = bus_cpu_read8(c -> sysbus, 0xFFFE);
    uint8_t hi = bus_cpu_read8(c -> sysbus, 0xFFFF);
    c -> PC = (uint16_t)(((uint16_t)hi << 8) | lo);

    return 7;
}

cpu create_cpu(bus b){
    cpu c = (cpu)malloc(sizeof(struct CPU));
    if(c == NULL){
        exit(EXIT_FAILURE);
    }
    memset(c, 0, sizeof(struct CPU));
    c -> sysbus = b;
    return c;
}

void cpu_destroy(cpu c){

}

void cpu_reset(cpu c){
    c -> S = 0xFD;      // Usual NES reset state
    c -> P = 0x34;      // IRQ disabled, set Z=I bit3/5 based on power-up
    uint8_t lo = bus_cpu_read8(c -> sysbus, 0xFFFC);
    uint8_t hi = bus_cpu_read8(c -> sysbus, 0xFFFD);
    c -> PC = (hi << 8) | lo;

    c -> cycles = 0;
    c -> stalled = false;
    c -> nmi_line = false;
    c -> irq_line = false;

    ppu_clear_nmi(c -> sysbus -> ppu);
}

void cpu_nmi(cpu c){
    c -> nmi_line = true;
}

void cpu_irq(cpu c){
    c -> irq_line = true;
}

int cpu_after_instruction(cpu c){
    if(c -> nmi_line){
        int extra = service_NMI(c);
        c -> nmi_line = false;
        return extra;
    } else if(c -> irq_line && (get_flag(c, FLAG_I)) == false){
        int extra = service_IRQ(c);
        c -> irq_line = false;
        return extra;
    }
    return 0;
}

uint8_t fetch8(cpu c){
    uint8_t v = bus_cpu_read8(c -> sysbus, c -> PC);
    c -> PC += 1;
    return v;
}

int execute_and_return_cycles(cpu c, uint8_t opcode){
    switch (opcode) {

        // ===== LDA =====
        case 0xA9: return op_LDA_imm(c);     // LDA #imm
        case 0xA5: return op_LDA_zp(c);      // LDA zp
        case 0xAD: return op_LDA_abs(c);     // LDA abs
        case 0xBD: return op_LDA_abs_x(c);   // LDA abs,X
        case 0xB9: return op_LDA_abs_y(c);   // LDA abs,Y
        case 0xA1: return op_LDA_ind_x(c);   // LDA (ind,X)
        case 0xB1: return op_LDA_ind_y(c);   // LDA (ind),Y

            // ===== LDX =====
        case 0xA2: return op_LDX_imm(c);     // LDX #imm
        case 0xA6: return op_LDX_zp(c);      // LDX zp
        case 0xB6: return op_LDX_zp_y(c);    // LDX zp,Y
        case 0xAE: return op_LDX_abs(c);     // LDX abs
        case 0xBE: return op_LDX_abs_y(c);   // LDX abs,Y

            // ===== LDY =====
        case 0xA0: return op_LDY_imm(c);     // LDY #imm
        case 0xA4: return op_LDY_zp(c);      // LDY zp
        case 0xB4: return op_LDY_zp_x(c);    // LDY zp,X
        case 0xAC: return op_LDY_abs(c);     // LDY abs
        case 0xBC: return op_LDY_abs_x(c);   // LDY abs,X

            // ===== STA =====
        case 0x85: return op_STA_zp(c);      // STA zp
        case 0x95: return op_STA_zp_x(c);    // STA zp,X
        case 0x8D: return op_STA_abs(c);     // STA abs
        case 0x9D: return op_STA_abs_x(c);   // STA abs,X
        case 0x99: return op_STA_abs_y(c);   // STA abs,Y
        case 0x81: return op_STA_ind_x(c);   // STA (ind,X)
        case 0x91: return op_STA_ind_y(c);   // STA (ind),Y

            // ===== AND =====
        case 0x29: return op_AND_imm(c);     // AND #imm
        case 0x25: return op_AND_zp(c);      // AND zp
        case 0x2D: return op_AND_abs(c);     // AND abs
        case 0x3D: return op_AND_abs_x(c);   // AND abs,X
        case 0x39: return op_AND_abs_y(c);   // AND abs,Y
        case 0x21: return op_AND_ind_x(c);   // AND (ind,X)
        case 0x31: return op_AND_ind_y(c);   // AND (ind),Y

            // ===== ORA =====
        case 0x09: return op_ORA_imm(c);     // ORA #imm
        case 0x05: return op_ORA_zp(c);      // ORA zp
        case 0x0D: return op_ORA_abs(c);     // ORA abs
        case 0x1D: return op_ORA_abs_x(c);   // ORA abs,X
        case 0x19: return op_ORA_abs_y(c);   // ORA abs,Y
        case 0x01: return op_ORA_ind_x(c);   // ORA (ind,X)
        case 0x11: return op_ORA_ind_y(c);   // ORA (ind),Y

            // ===== EOR =====
        case 0x49: return op_EOR_imm(c);     // EOR #imm
        case 0x45: return op_EOR_zp(c);      // EOR zp
        case 0x4D: return op_EOR_abs(c);     // EOR abs
        case 0x5D: return op_EOR_abs_x(c);   // EOR abs,X
        case 0x59: return op_EOR_abs_y(c);   // EOR abs,Y
        case 0x41: return op_EOR_ind_x(c);   // EOR (ind,X)
        case 0x51: return op_EOR_ind_y(c);   // EOR (ind),Y

            // ===== ADC =====
        case 0x69: return op_ADC_imm(c);     // ADC #imm
        case 0x65: return op_ADC_zp(c);      // ADC zp
        case 0x75: return op_ADC_zp_x(c);    // ADC zp,X
        case 0x6D: return op_ADC_abs(c);     // ADC abs
        case 0x7D: return op_ADC_abs_x(c);   // ADC abs,X
        case 0x79: return op_ADC_abs_y(c);   // ADC abs,Y
        case 0x61: return op_ADC_ind_x(c);   // ADC (ind,X)
        case 0x71: return op_ADC_ind_y(c);   // ADC (ind),Y

            // ===== SBC =====
        case 0xE9: return op_SBC_imm(c);     // SBC #imm
        case 0xE5: return op_SBC_zp(c);      // SBC zp
        case 0xF5: return op_SBC_zp_x(c);    // SBC zp,X
        case 0xED: return op_SBC_abs(c);     // SBC abs
        case 0xFD: return op_SBC_abs_x(c);   // (se implementi)
        case 0xF9: return op_SBC_abs_y(c);   // SBC abs,Y
        case 0xE1: return op_SBC_ind_x(c);   // SBC (ind,X)
        case 0xF1: return op_SBC_ind_y(c);   // (se implementi)

            // ===== CMP / CPX / CPY =====
        case 0xC9: return op_CMP_imm(c);     // CMP #imm
        case 0xC5: return op_CMP_zp(c);      // CMP zp
        case 0xCD: return op_CMP_abs(c);     // CMP abs

        case 0xE0: return op_CPX_imm(c);     // CPX #imm
        case 0xE4: return op_CPX_zp(c);      // CPX zp
        case 0xEC: return op_CPX_abs(c);     // CPX abs

        case 0xC0: return op_CPY_imm(c);     // CPY #imm
        case 0xC4: return op_CPY_zp(c);      // CPY zp
        case 0xCC: return op_CPY_abs(c);     // CPY abs

            // ===== INC/DEC =====
        case 0xE6: return op_INC_zp(c);      // INC zp
        case 0xF6: return op_INC_zp_x(c);    // INC zp,X
        case 0xEE: return op_INC_abs(c);     // INC abs
        case 0xFE: return op_INC_abs_x(c);   // INC abs,X

        case 0xC6: return op_DEC_zp(c);      // DEC zp
        case 0xD6: return op_DEC_zp_x(c);    // DEC zp,X
        case 0xCE: return op_DEC_abs(c);     // DEC abs
        case 0xDE: return op_DEC_abs_x(c);   // DEC abs,X

            // ===== Shifts/Rotates Accumulator =====
        case 0x0A: return op_ASL_A(c);       // ASL A
        case 0x4A: return op_LSR_A(c);       // LSR A
        case 0x2A: return op_ROL_A(c);       // ROL A
        case 0x6A: return op_ROR_A(c);       // ROR A

            // ===== Shifts/Rotates ZP =====
        case 0x06: return op_ASL_zp(c);      // ASL zp
        case 0x46: return op_LSR_zp(c);      // LSR zp
        case 0x26: return op_ROL_zp(c);      // ROL zp
        case 0x66: return op_ROR_zp(c);      // ROR zp

            // ===== BIT =====
        case 0x24: return op_BIT_zp(c);      // BIT zp
        case 0x2C: return op_BIT_abs(c);     // BIT abs

            // ===== Jumps / Subroutines =====
        case 0x4C: return op_JMP_abs(c);     // JMP abs
        case 0x6C: return op_JMP_ind(c);     // JMP (ind)
        case 0x20: return op_JSR(c);         // JSR abs
        case 0x60: return op_RTS(c);         // RTS

            // ===== Branches =====
        case 0xF0: return op_BEQ(c);         // BEQ
        case 0xD0: return op_BNE(c);         // BNE
        case 0xB0: return op_BCS(c);         // BCS
        case 0x90: return op_BCC(c);         // BCC
        case 0x30: return op_BMI(c);         // BMI
        case 0x10: return op_BPL(c);         // BPL
        case 0x70: return op_BVS(c);         // BVS
        case 0x50: return op_BVC(c);         // BVC

            // ===== Stack & Status =====
        case 0x48: return op_PHA(c);         // PHA
        case 0x68: return op_PLA(c);         // PLA
        case 0x08: return op_PHP(c);         // PHP
        case 0x28: return op_PLP(c);         // PLP

        case 0x38: return op_SEC(c);         // SEC
        case 0x18: return op_CLC(c);         // CLC
        case 0x78: return op_SEI(c);         // SEI
        case 0x58: return op_CLI(c);         // CLI
        case 0xB8: return op_CLV(c);         // CLV
        case 0xD8: return op_CLD(c);         // CLD
        case 0xF8: return op_SED(c);         // SED

        // =====================
        // Transfer & Stack
        // =====================
        // ===== Transfers =====
        case 0xAA: return op_TAX(c);        // TAX
        case 0xA8: return op_TAY(c);        // TAY
        case 0x8A: return op_TXA(c);        // TXA
        case 0x98: return op_TYA(c);        // TYA
        case 0xBA: return op_TSX(c);        // TSX
        case 0x9A: return op_TXS(c);        // TXS

        // ===== Index inc/dec =====
        case 0xE8: return op_INX(c);        // INX
        case 0xCA: return op_DEX(c);        // DEX
        case 0xC8: return op_INY(c);        // INY
        case 0x88: return op_DEY(c);        // DEY

            // ===== BRK / RTI / NOP =====
        case 0x00: return op_BRK(c);         // BRK
        case 0x40: return op_RTI(c);         // RTI
        case 0xEA: return op_NOP(c);         // NOP

            // LDA
        case 0xB5: return op_LDA_zp_x(c);   // LDA zp,X

// STX
        case 0x86: return op_STX_zp(c);     // STX zp
        case 0x96: return op_STX_zp_y(c);   // STX zp,Y
        case 0x8E: return op_STX_abs(c);    // STX abs

// STY
        case 0x84: return op_STY_zp(c);     // STY zp
        case 0x94: return op_STY_zp_x(c);   // STY zp,X
        case 0x8C: return op_STY_abs(c);    // STY abs

// Shifts/Rotates ZP,X
        case 0x16: return op_ASL_zp_x(c);   // ASL zp,X
        case 0x56: return op_LSR_zp_x(c);   // LSR zp,X
        case 0x36: return op_ROL_zp_x(c);   // ROL zp,X
        case 0x76: return op_ROR_zp_x(c);   // ROR zp,X

// Shifts/Rotates ABS / ABS,X
        case 0x0E: return op_ASL_abs(c);    // ASL abs
        case 0x1E: return op_ASL_abs_x(c);  // ASL abs,X
        case 0x4E: return op_LSR_abs(c);    // LSR abs
        case 0x5E: return op_LSR_abs_x(c);  // LSR abs,X
        case 0x2E: return op_ROL_abs(c);    // ROL abs
        case 0x3E: return op_ROL_abs_x(c);  // ROL abs,X
        case 0x6E: return op_ROR_abs(c);    // ROR abs
        case 0x7E: return op_ROR_abs_x(c);  // ROR abs,X

// CMP varianti mancanti
        case 0xD5: return op_CMP_zp_x(c);   // CMP zp,X
        case 0xDD: return op_CMP_abs_x(c);  // CMP abs,X
        case 0xD9: return op_CMP_abs_y(c);  // CMP abs,Y
        case 0xC1: return op_CMP_ind_x(c);  // CMP (ind,X)
        case 0xD1: return op_CMP_ind_y(c);  // CMP (ind),Y

            // ===== Default: NOP “undoc” =====
        default:   return 2;
    }
}

int cpu_step(cpu c){
    if(c -> stalled){
        // OAM DMA -> 513/514 total cycles handled by bus;
        // here if bus_tick_dma() says "one cycle consumed, return 1
        return bus_tick_dma_if_active(c -> sysbus, c -> cycles);
    }

    uint8_t opcode = fetch8(c);
    int base_cycles = execute_and_return_cycles(c, opcode);

    int extra = cpu_after_instruction(c);
    return base_cycles + extra;
}
