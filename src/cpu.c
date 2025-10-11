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
    push8(c, (uint8_t)(v & 0xFF));
    push8(c, (uint8_t)(v >> 8));
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

static inline uint16_t addr_abs_x(cpu c, uint8_t* crossed){
    uint16_t base=fetch_imm16(c);
    uint16_t eff=base + c->X;
    if(crossed) *crossed = page_crossed(base,eff);
    return eff;
}

static inline uint16_t addr_abs_y(cpu c, uint8_t* crossed){
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
    uint16_t hi_addr = (uint16_t)((prt & 0xFF00) | (prt + 1) & 0x00FF);
    uint8_t hi = cpu_read8(c, hi_addr);
    return (uint16_t)((uint16_t)hi << 8) | (uint16_t)lo;
}


inline void set_flag(cpu c, uint8_t mask, bool on){
    if(on) c -> P |= mask;
    else   c -> P &= ~mask;
}

inline void set_ZS(cpu c, uint8_t v){
    set_flag(c, FLAG_Z, (v == 0));
    set_flag(c, FLAG_N, ((v & 0x80) != 0));
}

static inline uint8_t adc_core(cpu c, uint8_t m){
    uint8_t a = c -> A;
    uint8_t carry_in = (uint8_t)((c -> P & FLAG_C) != 0 ? 1 : 0);
    uint16_t sum16 = (uint16_t)a + (uint16_t)m + (uint16_t)carry_in;
    uint8_t res = (uint8_t)(sum16 & 0xFF);

    set_flag(c, FLAG_C,(sum16 > 0xFF));

    bool v_on = (((~(a ^ m)) & (a & res) & 0x80) != 0);
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
    set_ZS(c, res);
}

bool get_flag(cpu c, uint8_t mask){
    return (c -> P & mask) != 0;
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
    /* switch (opcode) {
        case 0xA9: return op_LDA_imm(c);
        case 0xA5: return op_LDA_zp(c);
        case 0xAD: return op_LDA_abs(c);
            // ...
        case 0x00: return op_BRK(c);
        default: return op_NOP_undoc(c);
    } */
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
