//
// Created by Dario Bonfiglio on 10/11/25.
//

#include "headers/cpu.h"
#include "headers/CPUopcodes.h"

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
    c -> N = value & 0x80;
}

uint16_t read_address(cpu c, uint16_t addr);

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

void interrupt_sequence(cpu c, enum InterruptType type){
    if(c -> I && type != NMI && type != BRK_){
        return;
    }

    if(type == BRK_) ++c -> PC;

    push_stack(c, c -> PC >> 8);
    push_stack(c, c -> PC);

    uint8_t flags = c -> N << 7 | c -> V << 6 | 1 << 5 | (type == BRK_) << 4 | c -> D << 3 | c -> I << 2 | c -> Z << 1 | c -> C;
    push_stack(c, flags);

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

    c -> skip_cycles += 7;

}

bool execute_implied(cpu c, uint8_t opcode){
    switch (((enum operation_implied)(opcode)))
    {
        case NOP:
            break;
        case BRK:
            interrupt_sequence(c, BRK_);
            break;
        case JSR:
            push_stack(c, (uint8_t)((c -> PC + 1) >> 8));
            push_stack(c, (uint8_t)(c -> PC + 1));
            c -> PC = read_address(c, c -> PC);
            break;
        case RTS:
            c -> PC  = pull_stack(c);
            c -> PC |= pull_stack(c) << 8;
            ++c -> PC;
            break;
        case RTI:
        {
            uint8_t flags = pull_stack(c);
            c -> N        = flags & 0x80;
            c -> V        = flags & 0x40;
            c -> D        = flags & 0x8;
            c -> I        = flags & 0x4;
            c -> Z        = flags & 0x2;
            c -> C        = flags & 0x1;
        }
            c -> PC  = pull_stack(c);
            c -> PC |= pull_stack(c) << 8;
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
            uint8_t flags = c -> N << 7 | c -> V << 6 | 1 << 5 | // supposed to always be 1
                         1 << 4 |                       // PHP pushes with the B flag as 1, no matter what
                         c -> D << 3 | c -> I << 2 | c -> Z << 1 | c -> C;
            push_stack(c, flags);
        }
            break;
        case PLP:
        {
            uint8_t flags = pull_stack(c);
            c -> N        = flags & 0x80;
            c -> V        = flags & 0x40;
            c -> D        = flags & 0x8;
            c -> I        = flags & 0x4;
            c -> Z        = flags & 0x2;
            c -> C        = flags & 0x1;
        }
            break;
        case PHA:
            push_stack(c, c -> A);
            break;
        case PLA:
            c -> A = pull_stack(c);
            setZN(c, c -> A);
            break;
        case DEY:
            --c -> Y;
            setZN(c, c -> Y);
            break;
        case DEX:
            --c -> X;
            setZN(c, c -> X);
            break;
        case TAY:
            c -> Y = c -> A;
            setZN(c, c -> Y);
            break;
        case INY:
            ++c -> Y;
            setZN(c, c -> Y);
            break;
        case INX:
            ++c -> X;
            setZN(c, c -> X);
            break;
        case CLC:
            c -> C = false;
            break;
        case SEC:
            c -> C = true;
            break;
        case CLI:
            c -> I = false;
            break;
        case SEI:
            c -> I = true;
            break;
        case CLD:
            c -> D = false;
            break;
        case SED:
            c -> D = true;
            break;
        case TYA:
            c -> A = c -> Y;
            setZN(c, c -> A);
            break;
        case CLV:
            c -> V = false;
            break;
        case TXA:
            c -> A = c -> X;
            setZN(c, c -> A);
            break;
        case TXS:
            c -> SP = c -> X;
            break;
        case TAX:
            c -> X = c -> A;
            setZN(c, c -> X);
            break;
        case TSX:
            c -> X = c -> SP;
            setZN(c, c -> X);
            break;
        default:
            return false;
    };
    return true;
}
bool execute_branch(cpu c, uint8_t opcode){
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

        if (branch)
        {
            int8_t offset = bus_read(c -> bus, c -> PC++);
            ++c -> skip_cycles;
            uint16_t newPC = (uint16_t)(c -> PC + offset);
            skipPageCrossCycle(c, c -> PC, newPC);
            c -> PC = newPC;
        }
        else
            ++c -> PC;
        return true;
    }
    return false;
}

bool execute_type0(cpu c, uint8_t opcode){
    if ((opcode & INSTRUCTION_MODE_MASK) == 0x0)
    {
        uint16_t location = 0;
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
                // Address wraps around in the zero page
                location = (bus_read(c -> bus,c -> PC++) + c -> X) & 0xff;
                break;
            case AbsoluteIndexed:
                location  = read_address(c, c -> PC);
                c -> PC     += 2;
                skipPageCrossCycle(c, location, location + c -> X);
                location += c -> X;
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
            {
                uint8_t zero_addr = c -> X + bus_read(c -> bus,c -> PC++);
                // Addresses wrap in zero page mode, thus pass through a mask
                location       = bus_read(c -> bus,zero_addr & 0xff) | bus_read(c -> bus,(zero_addr + 1) & 0xff) << 8;
            }
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
            {
                uint8_t zero_addr = bus_read(c -> bus,c -> PC++);
                location       = bus_read(c -> bus,zero_addr & 0xff) | bus_read(c -> bus,(zero_addr + 1) & 0xff) << 8;
                if (op != STA)
                    skipPageCrossCycle(c, location, location + c -> Y);
                location += c -> Y;
            }
                break;
            case IndexedX:
                // Address wraps around in the zero page
                location = (bus_read(c -> bus,c -> PC++) + c -> X) & 0xff;
                break;
            case AbsoluteY:
                location  = read_address(c, c -> PC);
                c -> PC     += 2;
                if (op != STA)
                    skipPageCrossCycle(c, location, location + c -> Y);
                location += c -> Y;
                break;
            case AbsoluteX:
                location  = read_address(c, c -> PC);
                c -> PC     += 2;
                if (op != STA)
                    skipPageCrossCycle(c, location, location + c -> X);
                location += c -> X;
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
            {
                uint8_t          operand = bus_read(c -> bus,location);
                uint16_t sum     = c -> A + operand + c -> C;
                // Carry forward or UNSIGNED overflow
                c -> C                   = sum & 0x100;
                // SIGNED overflow, would only happen if the sign of sum is
                // different from BOTH the operands
                c -> V                   = (c -> A ^ sum) & (operand ^ sum) & 0x80;
                c -> A                   = sum;
                setZN(c, c -> A);
            }
                break;
            case STA:
                bus_write(c -> bus,location, c -> A);
                break;
            case LDA:
                c -> A = bus_read(c -> bus,location);
                setZN(c, c -> A);
                break;
            case SBC:
            {
                uint16_t subtrahend = bus_read(c -> bus,location), diff = c -> A - subtrahend - !c -> C;
                c -> C = !(diff & 0x100);
                c -> V = (c -> A ^ diff) & (~subtrahend ^ diff) & 0x80;
                c -> A = diff;
                setZN(c, diff);
            }
                break;
            case CMP:
            {
                uint16_t diff = c -> A - bus_read(c -> bus,location);
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


bool execute_type2(cpu c, uint8_t opcode){
    if ((opcode & INSTRUCTION_MODE_MASK) == 2)
    {
        uint16_t location  = 0;
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
                location  = read_address(c, c -> PC);
                c -> PC     += 2;
                uint8_t index;
                if (op == LDX || op == STX)
                    index = c -> Y;
                else
                    index = c -> X;
                skipPageCrossCycle(c, location, location + index);
                location += index;
            }
                break;
            default:
                return false;
        }

        uint16_t operand = 0;
        switch (op)
        {
            case ASL:
            case ROL:
                if (addr_mode == Accumulator)
                {
                    bool prev_C   = c -> C;
                    c -> C           = c -> A & 0x80;
                    c -> A         <<= 1;
                    // If Rotating, set the bit-0 to the the previous carry
                    c -> A           = c -> A | (prev_C && (op == ROL));
                    setZN(c, c -> A);
                }
                else
                {
                    bool prev_C = c -> C;
                    operand     = bus_read(c -> bus,location);
                    c -> C         = operand & 0x80;
                    operand     = operand << 1 | (prev_C && (op == ROL));
                    setZN(c, operand);
                    bus_write(c -> bus,location, operand);
                }
                break;
            case LSR:
            case ROR:
                if (addr_mode == Accumulator)
                {
                    bool prev_C   = c -> C;
                    c -> C           = c -> A & 1;
                    c -> A         >>= 1;
                    // If Rotating, set the bit-7 to the previous carry
                    c -> A           = c -> A | (prev_C && (op == ROR)) << 7;
                    setZN(c, c -> A);
                }
                else
                {
                    bool prev_C = c -> C;
                    operand     = bus_read(c -> bus,location);
                    c -> C         = operand & 1;
                    operand     = operand >> 1 | (prev_C && (op == ROR)) << 7;
                    setZN(c, operand);
                    bus_write(c -> bus,location, operand);
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
            {
                uint8_t tmp = bus_read(c -> bus,location) - 1;
                setZN(c, tmp);
                bus_write(c -> bus,location, tmp);
            }
                break;
            case INC:
            {
                uint8_t tmp = bus_read(c -> bus,location) + 1;
                setZN(c, tmp);
                bus_write(c -> bus,location, tmp);
            }
                break;
            default:
                return false;
        }
        return true;
    }
    return false;
}

// Public

void cpu_init(cpu c, bus b){
    c -> bus = b;
    c -> irq_pulldowns = 0;
    c -> pending_NMI = false;
    c -> irq_handlers = malloc(4 * sizeof(struct IRQHandler)); // capacità iniziale
    c -> irq_handlers_size = 0;
    c -> irq_handlers_capacity = 4;
}

uint16_t read_address(cpu c, uint16_t addr){
    return bus_read(c -> bus, addr) | bus_read(c -> bus, addr + 1) << 8;
}

void cpu_step(cpu c){
    ++c -> cycles;
    if(c -> skip_cycles-- > 1) return;
    c -> skip_cycles = 0;

    if(c -> pending_NMI){
        interrupt_sequence(c, NMI);
        c -> pending_NMI = false;
        return;
    }else if(is_pending_IRQ(c)){
        interrupt_sequence(c, IRQ);
        return;
    }

    int psw = c -> N << 7 | c -> V << 6 | 1 << 5 | c -> D << 3 | c -> I << 2 | c -> Z << 1 | c -> C;
    printf(
            "%04X  %02X  A:%02X X:%02X Y:%02X P:%02X SP:%02X CYC:%3d",
            c -> PC, bus_read(c -> bus, c -> PC),
            c -> A, c -> X, c -> Y, psw, c -> SP,
            ((c -> cycles - 1) * 3) % 341
    );

    uint8_t opcode = bus_read(c -> bus, c -> PC++);

    uint8_t cycle_lenght = operation_cycles[opcode];

    if (cycle_lenght && (execute_implied(c, opcode) || execute_branch(c, opcode) || execute_type1(c, opcode) ||
                         execute_type2(c, opcode) || execute_type0(c, opcode))) {
        c -> skip_cycles += cycle_lenght;
    } else {
        perror("Unrecognized opcode: 0x%04X", opcode);
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
    c -> PC                                    = start_addr;
    c -> SP                                    = 0xfd; // documented startup state
}

void skip_OAM_DMA_cycles(cpu c){
    c -> skip_cycles += 513;
    c -> skip_cycles += (c -> cycles & 1);
}
void skip_DMC_DMA_cycles(cpu c){
    c -> skip_cycles += 3;
}

void nmi_interrupt(cpu c){
    c -> pending_NMI = true;
}

void add_irq_handler(cpu c, int bit) {
    if (c -> irq_handlers_size >= c -> irq_handlers_capacity) {
        c -> irq_handlers_capacity *= 2;
        c -> irq_handlers = realloc(c -> irq_handlers,
                                    c -> irq_handlers_capacity * sizeof(struct IRQHandler));
    }

    irq_handler handler = (irq_handler)malloc(sizeof(struct IRQHandler));
    if(!handler) exit(EXIT_FAILURE);
    irq_init(handler, bit, c);
    c -> irq_handlers[c -> irq_handlers_size++] = handler;
}

irq_handle create_IRQ_handler(cpu c){
    int bit = 1 << c -> irq_handlers_size;
    add_irq_handler(c, bit);
    return (irq_handle)c -> irq_handlers[c -> irq_handlers_size - 1];
}

void set_IRQ_pulldown(cpu c, int bit, bool state){
    int mask           = ~(1 << bit);
    c -> irq_pulldowns = (c -> irq_pulldowns & mask) | state;
}