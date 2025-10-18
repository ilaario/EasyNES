//
// Created by Dario Bonfiglio on 10/18/25.
//

#ifndef EASYNES_CPUOPCODES_H
#define EASYNES_CPUOPCODES_H

enum branch_on_flag
{
    Negative,
    Overflow,
    Carry,
    Zero
};

enum operation_1
{
    ORA,
    AND,
    EOR,
    ADC,
    STA,
    LDA,
    CMP,
    SBC,
};

enum addr_mode_1
{
    IndexedIndirectX,
    ZeroPage,
    Immediate,
    Absolute,
    IndirectY,
    IndexedX,
    AbsoluteY,
    AbsoluteX,
};

enum operation_2
{
    ASL,
    ROL,
    LSR,
    ROR,
    STX,
    LDX,
    DEC,
    INC,
};

enum addr_mode_2
{
    Immediate_,
    ZeroPage_,
    Accumulator,
    Absolute_,
    Indexed         = 5,
    AbsoluteIndexed = 7,
};

enum operation_0
{
    BIT = 1,
    STY = 4,
    LDY,
    CPY,
    CPX,
};

enum operation_implied
{
    NOP  = 0xea,
    BRK  = 0x00,
    JSR  = 0x20,
    RTI  = 0x40,
    RTS  = 0x60,

    JMP  = 0x4C,
    JMPI = 0x6C, // JMP Indirect

    PHP  = 0x08,
    PLP  = 0x28,
    PHA  = 0x48,
    PLA  = 0x68,

    DEY  = 0x88,
    DEX  = 0xca,
    TAY  = 0xa8,
    INY  = 0xc8,
    INX  = 0xe8,

    CLC  = 0x18,
    SEC  = 0x38,
    CLI  = 0x58,
    SEI  = 0x78,
    TYA  = 0x98,
    CLV  = 0xb8,
    CLD  = 0xd8,
    SED  = 0xf8,

    TXA  = 0x8a,
    TXS  = 0x9a,
    TAX  = 0xaa,
    TSX  = 0xba,
};

enum InterruptType
{
    IRQ,
    NMI,
    BRK_
};

// 0 implies unused opcode
static const int operation_cycles[0x100] = {
        // clang-format off
        7, 6, 0, 0, 0, 3, 5, 0, 3, 2, 2, 0, 0, 4, 6, 0,
        2, 5, 0, 0, 0, 4, 6, 0, 2, 4, 0, 0, 0, 4, 7, 0,
        6, 6, 0, 0, 3, 3, 5, 0, 4, 2, 2, 0, 4, 4, 6, 0,
        2, 5, 0, 0, 0, 4, 6, 0, 2, 4, 0, 0, 0, 4, 7, 0,
        6, 6, 0, 0, 0, 3, 5, 0, 3, 2, 2, 0, 3, 4, 6, 0,
        2, 5, 0, 0, 0, 4, 6, 0, 2, 4, 0, 0, 0, 4, 7, 0,
        6, 6, 0, 0, 0, 3, 5, 0, 4, 2, 2, 0, 5, 4, 6, 0,
        2, 5, 0, 0, 0, 4, 6, 0, 2, 4, 0, 0, 0, 4, 7, 0,
        0, 6, 0, 0, 3, 3, 3, 0, 2, 0, 2, 0, 4, 4, 4, 0,
        2, 6, 0, 0, 4, 4, 4, 0, 2, 5, 2, 0, 0, 5, 0, 0,
        2, 6, 2, 0, 3, 3, 3, 0, 2, 2, 2, 0, 4, 4, 4, 0,
        2, 5, 0, 0, 4, 4, 4, 0, 2, 4, 2, 0, 4, 4, 4, 0,
        2, 6, 0, 0, 3, 3, 5, 0, 2, 2, 2, 0, 4, 4, 6, 0,
        2, 5, 0, 0, 0, 4, 6, 0, 2, 4, 0, 0, 0, 4, 7, 0,
        2, 6, 0, 0, 3, 3, 5, 0, 2, 2, 2, 2, 4, 4, 6, 0,
        2, 5, 0, 0, 0, 4, 6, 0, 2, 4, 0, 0, 0, 4, 7, 0,
        // clang-format on
};

#endif //EASYNES_CPUOPCODES_H
