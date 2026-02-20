//
// Created by Dario Bonfiglio on 10/11/25.
//

#include "headers/cpu.h"
#include "headers/CPUopcodes.h"
#include <stdio.h>

void irq_init(irq_handler irq, int bit, cpu c){
    irq -> c = c;
    irq -> bit = bit;
    irq -> irq_handle.pull = pull;
    irq -> irq_handle.release = release;
}

void release(irq_handle irq){
    irq_handler q = (irq_handler)irq;
    set_IRQ_pulldown(q -> c, q -> bit, false);
}

void pull(irq_handle irq){
    irq_handler q = (irq_handler)irq;
    set_IRQ_pulldown(q -> c, q -> bit, true);
}

// CPU
// Private

void setZN(cpu c, uint8_t value){
    c -> Z = !value;
    c -> N = (value & 0x80) != 0;
}

uint16_t read_address(cpu c, uint16_t addr);
static void interrupt_sequence(cpu c, enum InterruptType type, bool force_irq_mask, bool add_cycle_penalty);

static inline void clear_poll_schedule(cpu c) {
    c -> poll_count = 0;
    for (int i = 0; i < CPU_MAX_POLL_POINTS; ++i) c -> poll_cycles[i] = -1;
}

static inline void schedule_poll_abs(cpu c, int abs_cycle) {
    if (!c || c -> poll_count >= CPU_MAX_POLL_POINTS) return;
    for (int i = 0; i < c -> poll_count; ++i) {
        if (c -> poll_cycles[i] == abs_cycle) return;
    }
    c -> poll_cycles[c -> poll_count++] = abs_cycle;
}

static inline void schedule_poll_in(cpu c, int cycles_ahead) {
    schedule_poll_abs(c, c -> cycles + cycles_ahead);
}

static inline void schedule_i_update(cpu c, bool value, int apply_cycle) {
    c -> pending_i_update = true;
    c -> pending_i_value = value;
    c -> pending_i_apply_cycle = apply_cycle;
}

static inline void apply_pending_i_update(cpu c) {
    if (!c -> pending_i_update) return;
    if (c -> cycles < c -> pending_i_apply_cycle) return;
    c -> I = c -> pending_i_value;
    c -> pending_i_update = false;
}

static inline void finalize_interrupt_sequence(cpu c) {
    if (!c -> interrupt_sequence_active) return;
    if (c -> cycles < c -> interrupt_sequence_end_cycle) return;
    if (c -> interrupt_sequence_hijacked) {
        c -> PC = read_address(c, NMI_VECTOR);
        c -> pending_NMI = false;
        c -> nmi_latched = false;
    }
    c -> interrupt_sequence_active = false;
    c -> interrupt_sequence_hijacked = false;
}

static inline void latch_interrupts_from_poll(cpu c) {
    if (c -> pending_NMI && c -> pending_nmi_delay == 0) {
        if (c -> interrupt_sequence_active &&
            c -> interrupt_sequence_type != NMI &&
            c -> cycles < c -> interrupt_sequence_end_cycle) {
            c -> interrupt_sequence_hijacked = true;
        } else {
            c -> nmi_latched = true;
        }
    }
    if (!c -> I && is_irq_line_low(c)) c -> irq_latched = true;
}

static inline void process_poll_events(cpu c) {
    for (int i = 0; i < c -> poll_count; ++i) {
        if (c -> poll_cycles[i] != c -> cycles) continue;
        latch_interrupts_from_poll(c);
        c -> poll_cycles[i] = -1;
    }
}

static inline bool service_latched_interrupt(cpu c) {
    if (c -> nmi_latched) {
        c -> nmi_latched = false;
        c -> pending_NMI = false;
        interrupt_sequence(c, NMI, true, true);
        return true;
    }
    if (c -> irq_latched) {
        c -> irq_latched = false;
        interrupt_sequence(c, IRQ, true, true);
        return true;
    }
    return false;
}

static inline void schedule_instruction_polls(cpu c, uint8_t opcode, uint8_t cycles,
                                              bool is_branch, bool branch_taken, bool branch_crossed) {
    if (cycles < 2) return;

    if (opcode == BRK) {
        schedule_poll_in(c, 1);
        schedule_poll_in(c, 2);
        schedule_poll_in(c, 3);
        schedule_poll_in(c, 4);
        return;
    }

    if (opcode == CLI || opcode == SEI || opcode == PLP) {
        schedule_poll_in(c, 1);
        return;
    }

    if (opcode == RTI) {
        schedule_poll_in(c, 5);
        return;
    }

    if (is_branch) {
        schedule_poll_in(c, 1);
        if (branch_taken && branch_crossed) schedule_poll_in(c, 3);
        return;
    }

    schedule_poll_in(c, (int)cycles - 1);
}

static inline uint8_t pack_flags(cpu c, bool brk_flag) {
    return (uint8_t)(((c -> N ? 1 : 0) << 7) |
                     ((c -> V ? 1 : 0) << 6) |
                     (1 << 5) |
                     ((brk_flag ? 1 : 0) << 4) |
                     ((c -> D ? 1 : 0) << 3) |
                     ((c -> I ? 1 : 0) << 2) |
                     ((c -> Z ? 1 : 0) << 1) |
                     ((c -> C ? 1 : 0)));
}

static inline void unpack_flags(cpu c, uint8_t flags) {
    c -> N = (flags & 0x80) != 0;
    c -> V = (flags & 0x40) != 0;
    c -> D = (flags & 0x08) != 0;
    c -> I = (flags & 0x04) != 0;
    c -> Z = (flags & 0x02) != 0;
    c -> C = (flags & 0x01) != 0;
}

static inline uint16_t read_zp_address(cpu c, uint8_t zp_addr) {
    return (uint16_t)bus_read(c -> bus, zp_addr) |
           ((uint16_t)bus_read(c -> bus, (uint8_t)(zp_addr + 1)) << 8);
}

static inline uint16_t indexed_dummy_addr(uint16_t base, uint8_t idx) {
    return (uint16_t)((base & 0xFF00) | ((base + idx) & 0x00FF));
}

static inline bool crosses_page(uint16_t a, uint16_t b) {
    return (a & 0xFF00) != (b & 0xFF00);
}

static inline uint16_t absolute_indexed_addr(cpu c, uint8_t index,
                                             bool is_write, bool add_page_cross_cycle) {
    uint16_t base = read_address(c, c -> PC);
    c -> PC += 2;
    uint16_t addr = (uint16_t)(base + index);
    bool cross = crosses_page(base, addr);

    if (is_write) {
        (void)bus_read(c -> bus, indexed_dummy_addr(base, index));
    } else if (add_page_cross_cycle && cross) {
        (void)bus_read(c -> bus, indexed_dummy_addr(base, index));
        c -> skip_cycles += 1;
    }

    return addr;
}

static inline uint16_t indirect_y_addr(cpu c, bool is_write, bool add_page_cross_cycle) {
    uint8_t zp = bus_read(c -> bus, c -> PC++);
    uint16_t base = read_zp_address(c, zp);
    uint16_t addr = (uint16_t)(base + c -> Y);
    bool cross = crosses_page(base, addr);

    if (is_write) {
        (void)bus_read(c -> bus, indexed_dummy_addr(base, c -> Y));
    } else if (add_page_cross_cycle && cross) {
        (void)bus_read(c -> bus, indexed_dummy_addr(base, c -> Y));
        c -> skip_cycles += 1;
    }

    return addr;
}

static inline uint16_t indirect_x_addr(cpu c) {
    uint8_t zp = bus_read(c -> bus, c -> PC++);
    (void)bus_read(c -> bus, zp);
    return read_zp_address(c, (uint8_t)(zp + c -> X));
}

static inline void implied_dummy_read(cpu c) {
    (void)bus_read(c -> bus, c -> PC);
}

static inline void stack_dummy_read(cpu c) {
    (void)bus_read(c -> bus, (uint16_t)(0x100 | c -> SP));
}

static inline void rmw_dummy_write(cpu c, uint16_t addr, uint8_t value) {
    // INC/DEC/RMW to $4014 should not trigger a first DMA, but still drive the data bus.
    if (addr == OAM_DMA) {
        if (c && c -> bus) c -> bus -> data_bus = value;
        return;
    }
    bus_write(c -> bus, addr, value);
}

static inline uint16_t unstable_store_addr(uint16_t base, uint8_t index, uint8_t value) {
    uint16_t addr = (uint16_t)(base + index);
    if (((uint16_t)(base & 0x00FF) + index) > 0x00FF) {
        addr = (uint16_t)((addr & 0x00FF) | ((uint16_t)value << 8));
    }
    return addr;
}

static inline bool dmc_rdy_recent(cpu c) {
    return c && c -> dmc_dma_recent_cycles > 0;
}

static inline bool sh_rdy_effect(cpu c) {
    if (dmc_rdy_recent(c)) return true;
    if (!c || !c -> bus || !c -> bus -> apu || !c -> bus -> apu -> dmc) return false;
    dmc d = c -> bus -> apu -> dmc;
    // Accuracy tests drive RDY low around SH* via DMC activity just before write.
    // Our CPU core is instruction-granular, so we gate this behavior on active DMC as well.
    return d -> change_enabled && d -> remaining_bytes > 0;
}

static inline void adc_op(cpu c, uint8_t operand) {
    uint16_t sum = (uint16_t)c -> A + operand + (c -> C ? 1 : 0);
    c -> C = sum > 0xFF;
    c -> V = ((~(c -> A ^ operand) & (c -> A ^ sum)) & 0x80) != 0;
    c -> A = (uint8_t)sum;
    setZN(c, c -> A);
}

static inline void sbc_op(cpu c, uint8_t operand) {
    uint16_t value = (uint16_t)operand ^ 0x00FF;
    uint16_t sum = (uint16_t)c -> A + value + (c -> C ? 1 : 0);
    c -> C = sum > 0xFF;
    c -> V = ((sum ^ c -> A) & (sum ^ value) & 0x80) != 0;
    c -> A = (uint8_t)sum;
    setZN(c, c -> A);
}

static inline uint8_t rmw_asl(cpu c, uint16_t addr) {
    uint8_t v = bus_read(c -> bus, addr);
    rmw_dummy_write(c, addr, v);
    c -> C = (v & 0x80) != 0;
    v <<= 1;
    setZN(c, v);
    bus_write(c -> bus, addr, v);
    return v;
}

static inline uint8_t rmw_lsr(cpu c, uint16_t addr) {
    uint8_t v = bus_read(c -> bus, addr);
    rmw_dummy_write(c, addr, v);
    c -> C = (v & 0x01) != 0;
    v >>= 1;
    setZN(c, v);
    bus_write(c -> bus, addr, v);
    return v;
}

static inline uint8_t rmw_rol(cpu c, uint16_t addr) {
    uint8_t v = bus_read(c -> bus, addr);
    rmw_dummy_write(c, addr, v);
    bool prev_c = c -> C;
    c -> C = (v & 0x80) != 0;
    v = (uint8_t)((v << 1) | (prev_c ? 1 : 0));
    setZN(c, v);
    bus_write(c -> bus, addr, v);
    return v;
}

static inline uint8_t rmw_ror(cpu c, uint16_t addr) {
    uint8_t v = bus_read(c -> bus, addr);
    rmw_dummy_write(c, addr, v);
    bool prev_c = c -> C;
    c -> C = (v & 0x01) != 0;
    v = (uint8_t)((v >> 1) | ((prev_c ? 1 : 0) << 7));
    setZN(c, v);
    bus_write(c -> bus, addr, v);
    return v;
}

static inline uint8_t rmw_inc(cpu c, uint16_t addr) {
    uint8_t v = bus_read(c -> bus, addr);
    rmw_dummy_write(c, addr, v);
    v += 1;
    setZN(c, v);
    bus_write(c -> bus, addr, v);
    return v;
}

static inline uint8_t rmw_dec(cpu c, uint16_t addr) {
    uint8_t v = bus_read(c -> bus, addr);
    rmw_dummy_write(c, addr, v);
    v -= 1;
    setZN(c, v);
    bus_write(c -> bus, addr, v);
    return v;
}

void push_stack(cpu c, uint8_t value) {
    bus_write(c -> bus, 0x100 | c -> SP, value);
    --c -> SP;
}

uint8_t pull_stack(cpu c){
    return bus_read(c -> bus, 0x100 | ++c -> SP);
}

void skipPageCrossCycle(cpu c, uint16_t a, uint16_t b){
    if ((a & 0xff00) != (b & 0xff00)) c -> skip_cycles += 1;
}

static void interrupt_sequence(cpu c, enum InterruptType type, bool force_irq_mask, bool add_cycle_penalty){
    if(type == IRQ && c -> I && !force_irq_mask){
        return;
    }
    if(type == BRK_) ++c -> PC;

    push_stack(c, c -> PC >> 8);
    push_stack(c, c -> PC);

    push_stack(c, pack_flags(c, type == BRK_));

    c -> I = true;

    switch (type) {
        case IRQ:
        case BRK_:
            c -> PC = read_address(c, IRQ_VECTOR);
            break;
        case NMI:
            c -> PC = read_address(c, NMI_VECTOR);
            break;
        default:
            perror("Unknown interrupt");
    }

    c -> interrupt_sequence_active = true;
    c -> interrupt_sequence_type = type;
    c -> interrupt_sequence_end_cycle = c -> cycles + 7;
    c -> interrupt_sequence_hijacked = false;
    if (type == IRQ || type == BRK_) {
        // Interrupt sequences can be hijacked by NMI if it is detected during early cycles.
        schedule_poll_in(c, 1);
        schedule_poll_in(c, 2);
        schedule_poll_in(c, 3);
        schedule_poll_in(c, 4);
    }
    if (add_cycle_penalty) c -> skip_cycles += 7;
}

bool execute_implied(cpu c, uint8_t opcode){
    switch (((enum operation_implied)(opcode)))
    {
        case NOP:
            implied_dummy_read(c);
            break;
        case BRK:
            implied_dummy_read(c);
            interrupt_sequence(c, BRK_, true, false);
            break;
        case JSR:
        {
            uint8_t low = bus_read(c -> bus, c -> PC++);
            stack_dummy_read(c);
            uint16_t ret = c -> PC;
            push_stack(c, (uint8_t)(ret >> 8));
            push_stack(c, (uint8_t)ret);
            uint8_t high = bus_read(c -> bus, c -> PC++);
            c -> PC = (uint16_t)low | ((uint16_t)high << 8);
        }
            break;
        case RTS:
            implied_dummy_read(c);
            stack_dummy_read(c);
            c -> PC  = pull_stack(c);
            c -> PC |= pull_stack(c) << 8;
            (void)bus_read(c -> bus, c -> PC);
            ++c -> PC;
            break;
        case RTI:
        {
            implied_dummy_read(c);
            stack_dummy_read(c);
            uint8_t flags = pull_stack(c);
            bool pulled_i = (flags & 0x04) != 0;
            bool old_i = c -> I;
            unpack_flags(c, flags);
            c -> I = old_i;
            // RTI pulls P before its final interrupt poll; update I one cycle before that poll.
            schedule_i_update(c, pulled_i, c -> cycles + 4);
            uint8_t pcl = pull_stack(c);
            uint8_t pch = pull_stack(c);
            c -> PC = (uint16_t)pcl | ((uint16_t)pch << 8);
        }
            break;
        case JMP:
            c -> PC = read_address(c, c -> PC);
            break;
        case JMPI:
        {
            uint16_t location = read_address(c, c -> PC);
            uint16_t Page     = location & 0xff00;
            c -> PC             = bus_read(c -> bus,location) | bus_read(c -> bus,Page | ((location + 1) & 0xff)) << 8;
        }
            break;
        case PHP:
        {
            implied_dummy_read(c);
            push_stack(c, pack_flags(c, true));
        }
            break;
        case PLP:
        {
            implied_dummy_read(c);
            stack_dummy_read(c);
            uint8_t flags = pull_stack(c);
            bool pulled_i = (flags & 0x04) != 0;
            bool old_i = c -> I;
            unpack_flags(c, flags);
            c -> I = old_i;
            // PLP polls before cycle 2; defer the I update to the instruction end.
            schedule_i_update(c, pulled_i, c -> cycles + 4);
        }
            break;
        case PHA:
            implied_dummy_read(c);
            push_stack(c, c -> A);
            break;
        case PLA:
            implied_dummy_read(c);
            stack_dummy_read(c);
            c -> A = pull_stack(c);
            setZN(c, c -> A);
            break;
        case DEY:
            implied_dummy_read(c);
            --c -> Y;
            setZN(c, c -> Y);
            break;
        case DEX:
            implied_dummy_read(c);
            --c -> X;
            setZN(c, c -> X);
            break;
        case TAY:
            implied_dummy_read(c);
            c -> Y = c -> A;
            setZN(c, c -> Y);
            break;
        case INY:
            implied_dummy_read(c);
            ++c -> Y;
            setZN(c, c -> Y);
            break;
        case INX:
            implied_dummy_read(c);
            ++c -> X;
            setZN(c, c -> X);
            break;
        case CLC:
            implied_dummy_read(c);
            c -> C = false;
            break;
        case SEC:
            implied_dummy_read(c);
            c -> C = true;
            break;
        case CLI:
            implied_dummy_read(c);
            schedule_i_update(c, false, c -> cycles + 2);
            break;
        case SEI:
            implied_dummy_read(c);
            schedule_i_update(c, true, c -> cycles + 2);
            break;
        case CLD:
            implied_dummy_read(c);
            c -> D = false;
            break;
        case SED:
            implied_dummy_read(c);
            c -> D = true;
            break;
        case TYA:
            implied_dummy_read(c);
            c -> A = c -> Y;
            setZN(c, c -> A);
            break;
        case CLV:
            implied_dummy_read(c);
            c -> V = false;
            break;
        case TXA:
            implied_dummy_read(c);
            c -> A = c -> X;
            setZN(c, c -> A);
            break;
        case TXS:
            implied_dummy_read(c);
            c -> SP = c -> X;
            break;
        case TAX:
            implied_dummy_read(c);
            c -> X = c -> A;
            setZN(c, c -> X);
            break;
        case TSX:
            implied_dummy_read(c);
            c -> X = c -> SP;
            setZN(c, c -> X);
            break;
        default:
            return false;
    };
    return true;
}
bool execute_branch(cpu c, uint8_t opcode, bool *branch_taken_out, bool *branch_crossed_out){
    if (branch_taken_out) *branch_taken_out = false;
    if (branch_crossed_out) *branch_crossed_out = false;
    if ((opcode & BRANCH_INSTR_MASK) == BRANCH_INSTR_MASK_RESULT)
    {
        bool branch = opcode & BRANCH_COND_MASK;

        switch (opcode >> BRANCH_ON_FLAG_SHIFT)
        {
            case Negative:
                branch = !(branch ^ c -> N);
                break;
            case Overflow:
                branch = !(branch ^ c -> V);
                break;
            case Carry:
                branch = !(branch ^ c -> C);
                break;
            case Zero:
                branch = !(branch ^ c -> Z);
                break;
            default:
                return false;
        }

        int8_t offset = (int8_t)bus_read(c -> bus, c -> PC++);
        if (branch)
        {
            ++c -> skip_cycles;
            (void)bus_read(c -> bus, c -> PC); // taken-branch dummy read
            uint16_t newPC = (uint16_t)(c -> PC + offset);
            bool crossed = crosses_page(c -> PC, newPC);
            if (crossed) {
                (void)bus_read(c -> bus, indexed_dummy_addr(c -> PC, (uint8_t)offset));
            }
            skipPageCrossCycle(c, c -> PC, newPC);
            c -> PC = newPC;
            if (branch_taken_out) *branch_taken_out = true;
            if (branch_crossed_out) *branch_crossed_out = crossed;
        }
        return true;
    }
    return false;
}

bool execute_type0(cpu c, uint8_t opcode){
    if ((opcode & INSTRUCTION_MODE_MASK) == 0x0)
    {
        uint16_t location = 0;
        uint16_t base = 0;
        switch ((enum addr_mode_2)((opcode & ADDR_MODE_MASK) >> ADDR_MODE_SHIFT))
        {
            case Immediate_:
                location = c -> PC++;
                break;
            case ZeroPage_:
                location = bus_read(c -> bus,c -> PC++);
                break;
            case Absolute_:
                location  = read_address(c, c -> PC);
                c -> PC     += 2;
                break;
            case Indexed:
            {
                uint8_t zp = bus_read(c -> bus, c -> PC++);
                (void)bus_read(c -> bus, zp); // indexed zero-page dummy read
                location = (uint8_t)(zp + c -> X);
            }
                break;
            case AbsoluteIndexed:
                base = read_address(c, c -> PC);
                c -> PC += 2;
                location = (uint16_t)(base + c -> X);
                if (crosses_page(base, location)) {
                    (void)bus_read(c -> bus, indexed_dummy_addr(base, c -> X));
                    c -> skip_cycles += 1;
                }
                break;
            default:
                return false;
        }
        uint16_t operand = 0;
        switch ((enum operation_0)((opcode & OPERATION_MASK) >> OPERATION_SHIFT))
        {
            case BIT:
                operand = bus_read(c -> bus,location);
                c -> Z     = !(c -> A & operand);
                c -> V     = operand & 0x40;
                c -> N     = operand & 0x80;
                break;
            case STY:
                bus_write(c -> bus,location, c -> Y);
                break;
            case LDY:
                c -> Y = bus_read(c -> bus,location);
                setZN(c, c -> Y);
                break;
            case CPY:
            {
                uint16_t diff = c -> Y - bus_read(c -> bus,location);
                c -> C                = !(diff & 0x100);
                setZN(c, diff);
            }
                break;
            case CPX:
            {
                uint16_t diff = c -> X - bus_read(c -> bus,location);
                c -> C                = !(diff & 0x100);
                setZN(c, diff);
            }
                break;
            default:
                return false;
        }

        return true;
    }
    return false;
}

bool execute_type1(cpu c, uint8_t opcode){
    if ((opcode & INSTRUCTION_MODE_MASK) == 0x1)
    {
        uint16_t location = 0;
        int op = (enum operation_1)((opcode & OPERATION_MASK) >> OPERATION_SHIFT);
        switch ((enum addr_mode_1)((opcode & ADDR_MODE_MASK) >> ADDR_MODE_SHIFT))
        {
            case IndexedIndirectX:
                location = indirect_x_addr(c);
                break;
            case ZeroPage:
                location = bus_read(c -> bus,c -> PC++);
                break;
            case Immediate:
                location = c -> PC++;
                break;
            case Absolute:
                location  = read_address(c, c -> PC);
                c -> PC     += 2;
                break;
            case IndirectY:
                location = indirect_y_addr(c, op == STA, op != STA);
                break;
            case IndexedX: {
                uint8_t zp = bus_read(c -> bus, c -> PC++);
                (void)bus_read(c -> bus, zp); // indexed zero-page dummy read
                location = (uint8_t)(zp + c -> X);
            }
                break;
            case AbsoluteY:
                location = absolute_indexed_addr(c, c -> Y, op == STA, op != STA);
                break;
            case AbsoluteX:
                location = absolute_indexed_addr(c, c -> X, op == STA, op != STA);
                break;
            default:
                return false;
        }

        switch (op)
        {
            case ORA:
                c -> A |= bus_read(c -> bus,location);
                setZN(c, c -> A);
                break;
            case AND:
                c -> A &= bus_read(c -> bus,location);
                setZN(c, c -> A);
                break;
            case EOR:
                c -> A ^= bus_read(c -> bus,location);
                setZN(c, c -> A);
                break;
            case ADC:
                adc_op(c, bus_read(c -> bus, location));
                break;
            case STA:
                bus_write(c -> bus,location, c -> A);
                break;
            case LDA:
                c -> A = bus_read(c -> bus,location);
                setZN(c, c -> A);
                break;
            case SBC:
                sbc_op(c, bus_read(c -> bus, location));
                break;
            case CMP:
            {
                uint16_t diff = c -> A - bus_read(c -> bus,location);
                c -> C                = !(diff & 0x100);
                setZN(c, (uint8_t)diff);
            }
                break;
            default:
                return false;
        }
        return true;
    }
    return false;
}


bool execute_type2(cpu c, uint8_t opcode){
    if ((opcode & INSTRUCTION_MODE_MASK) == 2)
    {
        uint16_t location  = 0;
        uint16_t base = 0;
        int    op        = (enum operation_2)((opcode & OPERATION_MASK) >> OPERATION_SHIFT);
        int    addr_mode = (enum addr_mode_2)((opcode & ADDR_MODE_MASK) >> ADDR_MODE_SHIFT);
        switch (addr_mode)
        {
            case Immediate_:
                location = c -> PC++;
                break;
            case ZeroPage_:
                location = bus_read(c -> bus,c -> PC++);
                break;
            case Accumulator:
                break;
            case Absolute_:
                location  = read_address(c, c -> PC);
                c -> PC     += 2;
                break;
            case Indexed:
            {
                location = bus_read(c -> bus,c -> PC++);
                (void)bus_read(c -> bus, (uint8_t)location); // indexed zero-page dummy read
                uint8_t index;
                if (op == LDX || op == STX)
                    index = c -> Y;
                else
                    index = c -> X;
                // The mask wraps address around zero page
                location = (location + index) & 0xff;
            }
                break;
            case AbsoluteIndexed:
            {
                base  = read_address(c, c -> PC);
                c -> PC += 2;
                uint8_t index;
                if (op == LDX || op == STX)
                    index = c -> Y;
                else
                    index = c -> X;
                location = (uint16_t)(base + index);
                if (op == LDX) {
                    if (crosses_page(base, location)) {
                        (void)bus_read(c -> bus, indexed_dummy_addr(base, index));
                        c -> skip_cycles += 1;
                    }
                } else {
                    // RMW absolute indexed always performs a dummy read on the wrapped address.
                    (void)bus_read(c -> bus, indexed_dummy_addr(base, index));
                }
            }
                break;
            default:
                return false;
        }

        switch (op)
        {
            case ASL:
            case ROL:
                if (addr_mode == Accumulator)
                {
                    implied_dummy_read(c);
                    bool prev_C   = c -> C;
                    c -> C           = c -> A & 0x80;
                    c -> A         <<= 1;
                    // If Rotating, set the bit-0 to the the previous carry
                    c -> A           = c -> A | (prev_C && (op == ROL));
                    setZN(c, c -> A);
                }
                else
                {
                    if (op == ASL) rmw_asl(c, location);
                    else rmw_rol(c, location);
                }
                break;
            case LSR:
            case ROR:
                if (addr_mode == Accumulator)
                {
                    implied_dummy_read(c);
                    bool prev_C   = c -> C;
                    c -> C           = c -> A & 1;
                    c -> A         >>= 1;
                    // If Rotating, set the bit-7 to the previous carry
                    c -> A           = c -> A | (prev_C && (op == ROR)) << 7;
                    setZN(c, c -> A);
                }
                else
                {
                    if (op == LSR) rmw_lsr(c, location);
                    else rmw_ror(c, location);
                }
                break;
            case STX:
                bus_write(c -> bus,location, c -> X);
                break;
            case LDX:
                c -> X = bus_read(c -> bus,location);
                setZN(c, c -> X);
                break;
            case DEC:
                rmw_dec(c, location);
                break;
            case INC:
                rmw_inc(c, location);
                break;
            default:
                return false;
        }
        return true;
    }
    return false;
}

static inline uint16_t zpx_addr(cpu c) {
    uint8_t zp = bus_read(c -> bus, c -> PC++);
    (void)bus_read(c -> bus, zp);
    return (uint8_t)(zp + c -> X);
}

static inline uint16_t zpy_addr(cpu c) {
    uint8_t zp = bus_read(c -> bus, c -> PC++);
    (void)bus_read(c -> bus, zp);
    return (uint8_t)(zp + c -> Y);
}

static bool execute_unofficial_nop(cpu c, uint8_t opcode) {
    switch (opcode) {
        case 0x1A: case 0x3A: case 0x5A: case 0x7A: case 0xDA: case 0xFA:
            implied_dummy_read(c);
            return true;

        case 0x80: case 0x82: case 0x89: case 0xC2: case 0xE2:
            (void)bus_read(c -> bus, c -> PC++);
            return true;

        case 0x04: case 0x44: case 0x64: {
            uint8_t zp = bus_read(c -> bus, c -> PC++);
            (void)bus_read(c -> bus, zp);
            return true;
        }
        case 0x14: case 0x34: case 0x54: case 0x74: case 0xD4: case 0xF4: {
            uint8_t zp = bus_read(c -> bus, c -> PC++);
            (void)bus_read(c -> bus, zp);
            (void)bus_read(c -> bus, (uint8_t)(zp + c -> X));
            return true;
        }
        case 0x0C: {
            uint16_t addr = read_address(c, c -> PC);
            c -> PC += 2;
            (void)bus_read(c -> bus, addr);
            return true;
        }
        case 0x1C: case 0x3C: case 0x5C: case 0x7C: case 0xDC: case 0xFC: {
            uint16_t base = read_address(c, c -> PC);
            c -> PC += 2;
            uint16_t addr = (uint16_t)(base + c -> X);
            if (crosses_page(base, addr)) {
                (void)bus_read(c -> bus, indexed_dummy_addr(base, c -> X));
                c -> skip_cycles += 1;
            }
            (void)bus_read(c -> bus, addr);
            return true;
        }
        default:
            return false;
    }
}

static uint16_t rmw_unofficial_addr(cpu c, uint8_t opcode) {
    switch (opcode) {
        case 0x03: case 0x23: case 0x43: case 0x63:
        case 0xC3: case 0xE3:
            return indirect_x_addr(c);
        case 0x07: case 0x27: case 0x47: case 0x67:
        case 0xC7: case 0xE7:
            return bus_read(c -> bus, c -> PC++);
        case 0x0F: case 0x2F: case 0x4F: case 0x6F:
        case 0xCF: case 0xEF: {
            uint16_t addr = read_address(c, c -> PC);
            c -> PC += 2;
            return addr;
        }
        case 0x13: case 0x33: case 0x53: case 0x73:
        case 0xD3: case 0xF3:
            return indirect_y_addr(c, true, false);
        case 0x17: case 0x37: case 0x57: case 0x77:
        case 0xD7: case 0xF7:
            return zpx_addr(c);
        case 0x1B: case 0x3B: case 0x5B: case 0x7B:
        case 0xDB: case 0xFB:
            return absolute_indexed_addr(c, c -> Y, true, false);
        case 0x1F: case 0x3F: case 0x5F: case 0x7F:
        case 0xDF: case 0xFF:
            return absolute_indexed_addr(c, c -> X, true, false);
        default:
            return 0;
    }
}

static bool execute_unofficial(cpu c, uint8_t opcode) {
    if (execute_unofficial_nop(c, opcode)) return true;

    // Group 1: SLO / RLA / SRE / RRA / DCP / ISC
    switch (opcode) {
        case 0x03: case 0x07: case 0x0F: case 0x13: case 0x17: case 0x1B: case 0x1F:
        case 0x23: case 0x27: case 0x2F: case 0x33: case 0x37: case 0x3B: case 0x3F:
        case 0x43: case 0x47: case 0x4F: case 0x53: case 0x57: case 0x5B: case 0x5F:
        case 0x63: case 0x67: case 0x6F: case 0x73: case 0x77: case 0x7B: case 0x7F:
        case 0xC3: case 0xC7: case 0xCF: case 0xD3: case 0xD7: case 0xDB: case 0xDF:
        case 0xE3: case 0xE7: case 0xEF: case 0xF3: case 0xF7: case 0xFB: case 0xFF: {
            uint16_t addr = rmw_unofficial_addr(c, opcode);
            uint8_t opclass = opcode & 0xE3;
            switch (opclass) {
                case 0x03: { // SLO = ASL + ORA
                    uint8_t v = rmw_asl(c, addr);
                    c -> A |= v;
                    setZN(c, c -> A);
                    return true;
                }
                case 0x23: { // RLA = ROL + AND
                    uint8_t v = rmw_rol(c, addr);
                    c -> A &= v;
                    setZN(c, c -> A);
                    return true;
                }
                case 0x43: { // SRE = LSR + EOR
                    uint8_t v = rmw_lsr(c, addr);
                    c -> A ^= v;
                    setZN(c, c -> A);
                    return true;
                }
                case 0x63: { // RRA = ROR + ADC
                    uint8_t v = rmw_ror(c, addr);
                    adc_op(c, v);
                    return true;
                }
                case 0xC3: { // DCP = DEC + CMP
                    uint8_t v = rmw_dec(c, addr);
                    uint16_t diff = (uint16_t)c -> A - v;
                    c -> C = !(diff & 0x100);
                    setZN(c, (uint8_t)diff);
                    return true;
                }
                case 0xE3: { // ISC = INC + SBC
                    uint8_t v = rmw_inc(c, addr);
                    sbc_op(c, v);
                    return true;
                }
                default:
                    break;
            }
        } break;
        default:
            break;
    }

    switch (opcode) {
        // SAX
        case 0x83: {
            bus_write(c -> bus, indirect_x_addr(c), (uint8_t)(c -> A & c -> X));
            return true;
        }
        case 0x87: {
            bus_write(c -> bus, bus_read(c -> bus, c -> PC++), (uint8_t)(c -> A & c -> X));
            return true;
        }
        case 0x8F: {
            uint16_t addr = read_address(c, c -> PC);
            c -> PC += 2;
            bus_write(c -> bus, addr, (uint8_t)(c -> A & c -> X));
            return true;
        }
        case 0x97: {
            bus_write(c -> bus, zpy_addr(c), (uint8_t)(c -> A & c -> X));
            return true;
        }

        // LAX
        case 0xA3: {
            uint8_t v = bus_read(c -> bus, indirect_x_addr(c));
            c -> A = c -> X = v;
            setZN(c, v);
            return true;
        }
        case 0xA7: {
            uint8_t v = bus_read(c -> bus, bus_read(c -> bus, c -> PC++));
            c -> A = c -> X = v;
            setZN(c, v);
            return true;
        }
        case 0xAF: {
            uint16_t addr = read_address(c, c -> PC);
            c -> PC += 2;
            uint8_t v = bus_read(c -> bus, addr);
            c -> A = c -> X = v;
            setZN(c, v);
            return true;
        }
        case 0xB3: {
            uint8_t v = bus_read(c -> bus, indirect_y_addr(c, false, true));
            c -> A = c -> X = v;
            setZN(c, v);
            return true;
        }
        case 0xB7: {
            uint8_t v = bus_read(c -> bus, zpy_addr(c));
            c -> A = c -> X = v;
            setZN(c, v);
            return true;
        }
        case 0xBF: {
            uint8_t v = bus_read(c -> bus, absolute_indexed_addr(c, c -> Y, false, true));
            c -> A = c -> X = v;
            setZN(c, v);
            return true;
        }

        // SHA/AHX, SHX, SHY, SHS/TAS
        case 0x93: {
            uint8_t zp = bus_read(c -> bus, c -> PC++);
            uint16_t base = read_zp_address(c, zp);
            (void)bus_read(c -> bus, indexed_dummy_addr(base, c -> Y));
            uint8_t raw = (uint8_t)(c -> A & c -> X);
            uint8_t value = (uint8_t)(raw & ((((base >> 8) + 1) & 0xFF)));
            bool rdy = sh_rdy_effect(c);
            uint16_t addr = rdy ? (uint16_t)(base + c -> Y) : unstable_store_addr(base, c -> Y, value);
            bus_write(c -> bus, addr, rdy ? raw : value);
            return true;
        }
        case 0x9F: {
            uint16_t base = read_address(c, c -> PC);
            c -> PC += 2;
            (void)bus_read(c -> bus, indexed_dummy_addr(base, c -> Y));
            uint8_t raw = (uint8_t)(c -> A & c -> X);
            uint8_t value = (uint8_t)(raw & ((((base >> 8) + 1) & 0xFF)));
            bool rdy = sh_rdy_effect(c);
            uint16_t addr = rdy ? (uint16_t)(base + c -> Y) : unstable_store_addr(base, c -> Y, value);
            bus_write(c -> bus, addr, rdy ? raw : value);
            return true;
        }
        case 0x9E: {
            uint16_t base = read_address(c, c -> PC);
            c -> PC += 2;
            (void)bus_read(c -> bus, indexed_dummy_addr(base, c -> Y));
            uint8_t raw = c -> X;
            uint8_t value = (uint8_t)(raw & ((((base >> 8) + 1) & 0xFF)));
            bool rdy = sh_rdy_effect(c);
            uint16_t addr = rdy ? (uint16_t)(base + c -> Y) : unstable_store_addr(base, c -> Y, value);
            bus_write(c -> bus, addr, rdy ? raw : value);
            return true;
        }
        case 0x9C: {
            uint16_t base = read_address(c, c -> PC);
            c -> PC += 2;
            (void)bus_read(c -> bus, indexed_dummy_addr(base, c -> X));
            uint8_t raw = c -> Y;
            uint8_t value = (uint8_t)(raw & ((((base >> 8) + 1) & 0xFF)));
            bool rdy = sh_rdy_effect(c);
            uint16_t addr = rdy ? (uint16_t)(base + c -> X) : unstable_store_addr(base, c -> X, value);
            bus_write(c -> bus, addr, rdy ? raw : value);
            return true;
        }
        case 0x9B: {
            uint16_t base = read_address(c, c -> PC);
            c -> PC += 2;
            (void)bus_read(c -> bus, indexed_dummy_addr(base, c -> Y));
            c -> SP = (uint8_t)(c -> A & c -> X);
            uint8_t raw = c -> SP;
            uint8_t value = (uint8_t)(raw & ((((base >> 8) + 1) & 0xFF)));
            bool rdy = sh_rdy_effect(c);
            uint16_t addr = rdy ? (uint16_t)(base + c -> Y) : unstable_store_addr(base, c -> Y, value);
            bus_write(c -> bus, addr, rdy ? raw : value);
            return true;
        }

        // LAE / LAS
        case 0xBB: {
            uint8_t v = bus_read(c -> bus, absolute_indexed_addr(c, c -> Y, false, true));
            v = (uint8_t)(v & c -> SP);
            c -> A = c -> X = c -> SP = v;
            setZN(c, v);
            return true;
        }

        // Immediate unofficial ALU
        case 0x0B:
        case 0x2B: { // ANC
            c -> A &= bus_read(c -> bus, c -> PC++);
            setZN(c, c -> A);
            c -> C = c -> N;
            return true;
        }
        case 0x4B: { // ASR / ALR
            c -> A &= bus_read(c -> bus, c -> PC++);
            c -> C = (c -> A & 0x01) != 0;
            c -> A >>= 1;
            setZN(c, c -> A);
            return true;
        }
        case 0x6B: { // ARR
            uint8_t v = (uint8_t)(c -> A & bus_read(c -> bus, c -> PC++));
            v = (uint8_t)((v >> 1) | ((c -> C ? 1 : 0) << 7));
            c -> A = v;
            setZN(c, c -> A);
            c -> C = (v & 0x40) != 0;
            c -> V = (((v >> 6) ^ (v >> 5)) & 1) != 0;
            return true;
        }
        case 0x8B: { // ANE
            uint8_t imm = bus_read(c -> bus, c -> PC++);
            c -> A = (uint8_t)((c -> A | 0xEE) & c -> X & imm);
            setZN(c, c -> A);
            return true;
        }
        case 0xAB: { // LXA
            uint8_t imm = bus_read(c -> bus, c -> PC++);
            c -> A = c -> X = (uint8_t)((c -> A | 0xEE) & imm);
            setZN(c, c -> A);
            return true;
        }
        case 0xCB: { // AXS
            uint8_t imm = bus_read(c -> bus, c -> PC++);
            uint8_t ax = (uint8_t)(c -> A & c -> X);
            uint16_t diff = (uint16_t)ax - imm;
            c -> X = (uint8_t)diff;
            c -> C = !(diff & 0x100);
            setZN(c, c -> X);
            return true;
        }
        case 0xEB: // unofficial SBC immediate
            sbc_op(c, bus_read(c -> bus, c -> PC++));
            return true;
        default:
            return false;
    }
}

// Public

void cpu_init(cpu c, bus b){
    c -> bus = b;
    if (b) b -> cpu_owner = c;
    c -> irq_pulldowns = 0;
    c -> pending_NMI = false;
    c -> pending_nmi_delay = 0;
    c -> dmc_dma_recent_cycles = 0;
    c -> irq_latched = false;
    c -> nmi_latched = false;
    c -> pending_i_update = false;
    c -> pending_i_value = false;
    c -> pending_i_apply_cycle = 0;
    c -> interrupt_sequence_active = false;
    c -> interrupt_sequence_type = 0;
    c -> interrupt_sequence_end_cycle = 0;
    c -> interrupt_sequence_hijacked = false;
    clear_poll_schedule(c);
    c -> irq_handlers = (irq_handler*)calloc(4, sizeof(irq_handler)); // capacità iniziale
    c -> irq_handlers_size = 0;
    c -> irq_handlers_capacity = 4;
}

uint16_t read_address(cpu c, uint16_t addr){
    return bus_read(c -> bus, addr) | bus_read(c -> bus, addr + 1) << 8;
}

void cpu_step(cpu c){
    if (c -> dmc_dma_recent_cycles > 0) c -> dmc_dma_recent_cycles -= 1;
    if (c -> pending_nmi_delay > 0) c -> pending_nmi_delay -= 1;
    ++c -> cycles;

    apply_pending_i_update(c);
    process_poll_events(c);
    finalize_interrupt_sequence(c);

    if(c -> skip_cycles-- > 1) return;
    c -> skip_cycles = 0;
    if (service_latched_interrupt(c)) return;

#ifdef DEBUGLOG
    int psw = pack_flags(c, false);
    printf(
            "%04X  %02X  A:%02X X:%02X Y:%02X P:%02X SP:%02X CYC:%3d",
            c -> PC, bus_read(c -> bus, c -> PC),
            c -> A, c -> X, c -> Y, psw, c -> SP,
            ((c -> cycles - 1) * 3) % 341
    );
#endif

    uint8_t opcode = bus_read(c -> bus, c -> PC++);

    uint8_t cycle_lenght = operation_cycles[opcode];
    bool branch_taken = false;
    bool branch_crossed = false;
    bool is_branch = false;
    clear_poll_schedule(c);

    bool executed =
        execute_unofficial(c, opcode) ||
        execute_implied(c, opcode) ||
        ((is_branch = execute_branch(c, opcode, &branch_taken, &branch_crossed))) ||
        execute_type1(c, opcode) ||
        execute_type2(c, opcode) ||
        execute_type0(c, opcode);

    if (cycle_lenght && executed) {
        schedule_instruction_polls(c, opcode, cycle_lenght, is_branch, branch_taken, branch_crossed);
        c -> skip_cycles += cycle_lenght;
    } else {
        fprintf(stderr, "Unrecognized opcode: 0x%02X at PC=%04X\n", opcode, (uint16_t)(c -> PC - 1));
    }
}

void cpu_reset(cpu c){
    cpu_addr_reset(c, read_address(c, RESET_VECTOR));
}

void cpu_addr_reset(cpu c, uint16_t start_addr){
    c -> skip_cycles = c -> cycles             = 0;
    c -> A = c -> X = c -> Y                   = 0;
    c -> I                                     = true;
    c -> C = c -> D = c -> N = c -> V = c -> Z = false;
    c -> pending_NMI                           = false;
    c -> pending_nmi_delay                     = 0;
    c -> dmc_dma_recent_cycles                 = 0;
    c -> irq_latched                           = false;
    c -> nmi_latched                           = false;
    c -> pending_i_update                      = false;
    c -> pending_i_value                       = false;
    c -> pending_i_apply_cycle                 = 0;
    c -> interrupt_sequence_active             = false;
    c -> interrupt_sequence_type               = 0;
    c -> interrupt_sequence_end_cycle          = 0;
    c -> interrupt_sequence_hijacked           = false;
    clear_poll_schedule(c);
    c -> irq_pulldowns                         = 0;
    c -> PC                                    = start_addr;
    c -> SP                                    = 0xfd; // documented startup state
}

void skip_OAM_DMA_cycles(cpu c){
    c -> skip_cycles += 513;
    c -> skip_cycles += (c -> cycles & 1);
}
void skip_DMC_DMA_cycles(cpu c, int cycles){
    if (!c || cycles <= 0) return;
    c -> skip_cycles += cycles;
    c -> dmc_dma_recent_cycles = cycles + 4;
}

void nmi_interrupt(cpu c){
    c -> pending_NMI = true;
    if (c -> pending_nmi_delay == 0) c -> pending_nmi_delay = 2;
}

void add_irq_handler(cpu c, int bit) {
    if (c -> irq_handlers_size >= c -> irq_handlers_capacity) {
        c -> irq_handlers_capacity *= 2;
        c -> irq_handlers = (irq_handler*)realloc(c -> irq_handlers,
                                    c -> irq_handlers_capacity * sizeof(irq_handler));
    }

    irq_handler handler = (irq_handler)malloc(sizeof(struct IRQHandler));
    if(!handler) exit(EXIT_FAILURE);
    irq_init(handler, bit, c);
    c -> irq_handlers[c -> irq_handlers_size++] = handler;
}

irq_handle create_IRQ_handler(cpu c){
    if (c -> irq_handlers_size >= (int)(sizeof(int) * 8 - 1)) return NULL;
    int bit = 1 << c -> irq_handlers_size;
    add_irq_handler(c, bit);
    return (irq_handle)c -> irq_handlers[c -> irq_handlers_size - 1];
}

void set_IRQ_pulldown(cpu c, int bit, bool state){
    if (state) c -> irq_pulldowns |= bit;
    else c -> irq_pulldowns &= ~bit;
}
