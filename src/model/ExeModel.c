#include "ExeModel.h"
#include "../Hash.h"

// The parser's tables and constants. (They are file scope in C; in paq8px
// they are members of the ExeModel class.)

// formats
enum InstructionFormat {
    // encoding mode
    fNM = 0x0, // no ModRM
    fAM = 0x1, // no ModRM, "address mode" (jumps or direct addresses)
    fMR = 0x2, // ModRM present
    fMEXTRA = 0x3, // ModRM present, includes extra bits for opcode
    fMODE = 0x3, // bitmask for mode

    // no ModRM: size of immediate operand
    fNI = 0x0, // no immediate
    fBI = 0x4, // byte immediate
    fWI = 0x8, // word immediate
    fDI = 0xc, // dword immediate
    fTYPE = 0xc, // type mask

    // address mode: type of address operand
    fAD = 0x0, // absolute address
    fDA = 0x4, // dword absolute jump target
    fBR = 0x8, // byte relative jump target
    fDR = 0xc, // dword relative jump target

    // others
    fERR = 0xf, // denotes invalid opcodes
};

/**< 1 byte opcodes */
static const uint8_t table1[256] = {
    // 0       1       2       3       4       5       6       7       8       9       a       b       c       d       e       f
    fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fNM | fBI, fNM | fDI, fNM | fNI, fNM | fNI, fMR | fNI, fMR | fNI, fMR | fNI,
    fMR | fNI, fNM | fBI, fNM | fDI, fNM | fNI, fNM | fNI, // 0
    fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fNM | fBI, fNM | fDI, fNM | fNI, fNM | fNI, fMR | fNI, fMR | fNI, fMR | fNI,
    fMR | fNI, fNM | fBI, fNM | fDI, fNM | fNI, fNM | fNI, // 1
    fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fNM | fBI, fNM | fDI, fNM | fNI, fNM | fNI, fMR | fNI, fMR | fNI, fMR | fNI,
    fMR | fNI, fNM | fBI, fNM | fDI, fNM | fNI, fNM | fNI, // 2
    fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fNM | fBI, fNM | fDI, fNM | fNI, fNM | fNI, fMR | fNI, fMR | fNI, fMR | fNI,
    fMR | fNI, fNM | fBI, fNM | fDI, fNM | fNI, fNM | fNI, // 3

    fNM | fNI, fNM | fNI, fNM | fNI, fNM | fNI, fNM | fNI, fNM | fNI, fNM | fNI, fNM | fNI, fNM | fNI, fNM | fNI, fNM | fNI,
    fNM | fNI, fNM | fNI, fNM | fNI, fNM | fNI, fNM | fNI, // 4
    fNM | fNI, fNM | fNI, fNM | fNI, fNM | fNI, fNM | fNI, fNM | fNI, fNM | fNI, fNM | fNI, fNM | fNI, fNM | fNI, fNM | fNI,
    fNM | fNI, fNM | fNI, fNM | fNI, fNM | fNI, fNM | fNI, // 5
    fNM | fNI, fNM | fNI, fMR | fNI, fMR | fNI, fNM | fNI, fNM | fNI, fNM | fNI, fNM | fNI, fNM | fDI, fMR | fDI, fNM | fBI,
    fMR | fBI, fNM | fNI, fNM | fNI, fNM | fNI, fNM | fNI, // 6
    fAM | fBR, fAM | fBR, fAM | fBR, fAM | fBR, fAM | fBR, fAM | fBR, fAM | fBR, fAM | fBR, fAM | fBR, fAM | fBR, fAM | fBR,
    fAM | fBR, fAM | fBR, fAM | fBR, fAM | fBR, fAM | fBR, // 7

    fMR | fBI, fMR | fDI, fMR | fBI, fMR | fBI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI,
    fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, // 8
    fNM | fNI, fNM | fNI, fNM | fNI, fNM | fNI, fNM | fNI, fNM | fNI, fNM | fNI, fNM | fNI, fNM | fNI, fNM | fNI, fAM | fDA,
    fNM | fNI, fNM | fNI, fNM | fNI, fNM | fNI, fNM | fNI, // 9
    fAM | fAD, fAM | fAD, fAM | fAD, fAM | fAD, fNM | fNI, fNM | fNI, fNM | fNI, fNM | fNI, fNM | fBI, fNM | fDI, fNM | fNI,
    fNM | fNI, fNM | fNI, fNM | fNI, fNM | fNI, fNM | fNI, // a
    fNM | fBI, fNM | fBI, fNM | fBI, fNM | fBI, fNM | fBI, fNM | fBI, fNM | fBI, fNM | fBI, fNM | fDI, fNM | fDI, fNM | fDI,
    fNM | fDI, fNM | fDI, fNM | fDI, fNM | fDI, fNM | fDI, // b

    fMR | fBI, fMR | fBI, fNM | fWI, fNM | fNI, fMR | fNI, fMR | fNI, fMR | fBI, fMR | fDI, fNM | fBI, fNM | fNI, fNM | fWI,
    fNM | fNI, fNM | fNI, fNM | fBI, fERR, fNM | fNI, // c
    fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fNM | fBI, fNM | fBI, fNM | fNI, fNM | fNI, fMR | fNI, fMR | fNI, fMR | fNI,
    fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, // d
    fAM | fBR, fAM | fBR, fAM | fBR, fAM | fBR, fNM | fBI, fNM | fBI, fNM | fBI, fNM | fBI, fAM | fDR, fAM | fDR, fAM | fAD,
    fAM | fBR, fNM | fNI, fNM | fNI, fNM | fNI, fNM | fNI, // e
    fNM | fNI, fERR, fNM | fNI, fNM | fNI, fNM | fNI, fNM | fNI, fMEXTRA, fMEXTRA, fNM | fNI, fNM | fNI, fNM | fNI, fNM | fNI,
    fNM | fNI, fNM | fNI, fMEXTRA, fMEXTRA, // f
};

/**< 2 byte opcodes */
static const uint8_t table2[256] = {
    // 0       1       2       3       4       5       6       7       8       9       a       b       c       d       e       f
    fERR, fERR, fERR, fERR, fERR, fERR, fNM | fNI, fERR, fNM | fNI, fNM | fNI, fERR, fERR, fERR, fERR, fERR, fERR, // 0
    fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fERR, fERR, fERR, fERR, fERR,
    fERR, fERR, // 1
    fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fERR, fERR, fERR, fERR, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI,
    fMR | fNI, fMR | fNI, fMR | fNI, // 2
    fNM | fNI, fNM | fNI, fNM | fNI, fNM | fNI, fNM | fNI, fNM | fNI, fERR, fNM | fNI, fERR, fERR, fERR, fERR, fERR, fERR, fERR,
    fERR, // 3

    fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI,
    fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, // 4
    fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI,
    fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, // 5
    fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI,
    fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, // 6
    fMR | fBI, fMR | fBI, fMR | fBI, fMR | fBI, fMR | fNI, fMR | fNI, fMR | fNI, fNM | fNI, fERR, fERR, fERR, fERR, fERR, fERR,
    fMR | fNI, fMR | fNI, // 7

    fAM | fDR, fAM | fDR, fAM | fDR, fAM | fDR, fAM | fDR, fAM | fDR, fAM | fDR, fAM | fDR, fAM | fDR, fAM | fDR, fAM | fDR,
    fAM | fDR, fAM | fDR, fAM | fDR, fAM | fDR, fAM | fDR, // 8
    fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI,
    fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, // 9
    fNM | fNI, fNM | fNI, fNM | fNI, fMR | fNI, fMR | fBI, fMR | fNI, fMR | fNI, fMR | fNI, fERR, fERR, fERR, fMR | fNI, fMR | fBI,
    fMR | fNI, fERR, fMR | fNI, // a
    fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fERR, fERR, fERR, fMR | fNI, fMR | fNI,
    fMR | fNI, fMR | fNI, fMR | fNI, // b

    fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fNM | fNI, fNM | fNI, fNM | fNI,
    fNM | fNI, fNM | fNI, fNM | fNI, fNM | fNI, fNM | fNI, // c
    fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI,
    fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, // d
    fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI,
    fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, // e
    fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI,
    fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fERR, // f
};

/**< 3 byte opcodes 0F38XX */
static const uint8_t table3_38[256] = {
    // 0       1       2       3       4       5       6       7       8       9       a       b       c       d       e       f
    fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI,
    fMR | fNI, fERR, fERR, fERR, fERR, // 0
    fMR | fNI, fERR, fERR, fERR, fMR | fNI, fMR | fNI, fERR, fMR | fNI, fERR, fERR, fERR, fERR, fMR | fNI, fMR | fNI, fMR | fNI,
    fERR, // 1
    fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fERR, fERR, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fERR,
    fERR, fERR, fERR, // 2
    fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fERR, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI,
    fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, // 3
    fMR | fNI, fMR | fNI, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, // 4
    fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, // 5
    fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, // 6
    fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, // 7
    fMR | fNI, fMR | fNI, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, // 8
    fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, // 9
    fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, // a
    fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, // b
    fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, // c
    fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, // d
    fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, // e
    fMR | fNI, fMR | fNI, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, // f
};

/**< 3 byte opcodes 0F3AXX */
static const uint8_t table3_3A[256] = {
    // 0       1       2       3       4       5       6       7       8       9       a       b       c       d       e       f
    fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fMR | fBI, fMR | fBI, fMR | fBI, fMR | fBI, fMR | fBI, fMR | fBI, fMR | fBI,
    fMR | fBI, // 0
    fERR, fERR, fERR, fERR, fMR | fBI, fMR | fBI, fMR | fBI, fMR | fBI, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, // 1
    fMR | fBI, fMR | fBI, fMR | fBI, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, // 2
    fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, // 3
    fMR | fBI, fMR | fBI, fMR | fBI, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, // 4
    fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, // 5
    fMR | fBI, fMR | fBI, fMR | fBI, fMR | fBI, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, // 6
    fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, // 7
    fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, // 8
    fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, // 9
    fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, // a
    fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, // b
    fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, // c
    fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, // d
    fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, // e
    fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, // f
};

/**< escape opcodes using ModRM byte to get more variants */
static const uint8_t tableX[32] = {
    // 0       1       2       3       4       5       6       7
    fMR | fBI, fERR, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, // escapes for 0xf6
    fMR | fDI, fERR, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, // escapes for 0xf7
    fMR | fNI, fMR | fNI, fERR, fERR, fERR, fERR, fERR, fERR, // escapes for 0xfe
    fMR | fNI, fMR | fNI, fMR | fNI, fERR, fMR | fNI, fERR, fMR | fNI, fERR, // escapes for 0xff
};

static const uint8_t invalidX64Ops[19] = {0x06, 0x07, 0x16, 0x17, 0x1E, 0x1F, 0x27, 0x2F, 0x37, 0x3F, 0x60, 0x61, 0x62, 0x82, 0x9A, 0xD4, 0xD5, 0xD6, 0xEA,};
static const uint8_t x64Prefixes[8] = {0x26, 0x2E, 0x36, 0x3E, 0x9B, 0xF0, 0xF2, 0xF3,};

enum InstructionCategory {
    OP_INVALID = 0,
    OP_PREFIX_SEGREG = 1,
    OP_PREFIX = 2,
    OP_PREFIX_X87FPU = 3,
    OP_GEN_DATAMOV = 4,
    OP_GEN_STACK = 5,
    OP_GEN_CONVERSION = 6,
    OP_GEN_ARITH_DECIMAL = 7,
    OP_GEN_ARITH_BINARY = 8,
    OP_GEN_LOGICAL = 9,
    OP_GEN_SHF_ROT = 10,
    OP_GEN_BIT = 11,
    OP_GEN_BRANCH = 12,
    OP_GEN_BRANCH_COND = 13,
    OP_GEN_BREAK = 14,
    OP_GEN_STRING = 15,
    OP_GEN_INOUT = 16,
    OP_GEN_FLAG_CONTROL = 17,
    OP_GEN_SEGREG = 18,
    OP_GEN_CONTROL = 19,
    OP_SYSTEM = 20,
    OP_X87_DATAMOV = 21,
    OP_X87_ARITH = 22,
    OP_X87_COMPARISON = 23,
    OP_X87_TRANSCENDENTAL = 24,
    OP_X87_LOAD_CONSTANT = 25,
    OP_X87_CONTROL = 26,
    OP_X87_CONVERSION = 27,
    OP_STATE_MANAGEMENT = 28,
    OP_MMX = 29,
    OP_SSE = 30,
    OP_SSE_DATAMOV = 31,
};

static const uint8_t typeOp1[256] = {
  OP_GEN_ARITH_BINARY, OP_GEN_ARITH_BINARY, OP_GEN_ARITH_BINARY, OP_GEN_ARITH_BINARY, //03
  OP_GEN_ARITH_BINARY, OP_GEN_ARITH_BINARY, OP_GEN_STACK, OP_GEN_STACK, //07
  OP_GEN_LOGICAL, OP_GEN_LOGICAL, OP_GEN_LOGICAL, OP_GEN_LOGICAL, //0B
  OP_GEN_LOGICAL, OP_GEN_LOGICAL, OP_GEN_STACK, OP_PREFIX, //0F
  OP_GEN_ARITH_BINARY, OP_GEN_ARITH_BINARY, OP_GEN_ARITH_BINARY, OP_GEN_ARITH_BINARY, //13
  OP_GEN_ARITH_BINARY, OP_GEN_ARITH_BINARY, OP_GEN_STACK, OP_GEN_STACK, //17
  OP_GEN_ARITH_BINARY, OP_GEN_ARITH_BINARY, OP_GEN_ARITH_BINARY, OP_GEN_ARITH_BINARY, //1B
  OP_GEN_ARITH_BINARY, OP_GEN_ARITH_BINARY, OP_GEN_STACK, OP_GEN_STACK, //1F
  OP_GEN_LOGICAL, OP_GEN_LOGICAL, OP_GEN_LOGICAL, OP_GEN_LOGICAL, //23
  OP_GEN_LOGICAL, OP_GEN_LOGICAL, OP_PREFIX_SEGREG, OP_GEN_ARITH_DECIMAL, //27
  OP_GEN_ARITH_BINARY, OP_GEN_ARITH_BINARY, OP_GEN_ARITH_BINARY, OP_GEN_ARITH_BINARY, //2B
  OP_GEN_ARITH_BINARY, OP_GEN_ARITH_BINARY, OP_PREFIX_SEGREG, OP_GEN_ARITH_DECIMAL, //2F
  OP_GEN_LOGICAL, OP_GEN_LOGICAL, OP_GEN_LOGICAL, OP_GEN_LOGICAL, //33
  OP_GEN_LOGICAL, OP_GEN_LOGICAL, OP_PREFIX_SEGREG, OP_GEN_ARITH_DECIMAL, //37
  OP_GEN_ARITH_BINARY, OP_GEN_ARITH_BINARY, OP_GEN_ARITH_BINARY, OP_GEN_ARITH_BINARY, //3B
  OP_GEN_ARITH_BINARY, OP_GEN_ARITH_BINARY, OP_PREFIX_SEGREG, OP_GEN_ARITH_DECIMAL, //3F
  OP_GEN_ARITH_BINARY, OP_GEN_ARITH_BINARY, OP_GEN_ARITH_BINARY, OP_GEN_ARITH_BINARY, //43
  OP_GEN_ARITH_BINARY, OP_GEN_ARITH_BINARY, OP_GEN_ARITH_BINARY, OP_GEN_ARITH_BINARY, //47
  OP_GEN_ARITH_BINARY, OP_GEN_ARITH_BINARY, OP_GEN_ARITH_BINARY, OP_GEN_ARITH_BINARY, //4B
  OP_GEN_ARITH_BINARY, OP_GEN_ARITH_BINARY, OP_GEN_ARITH_BINARY, OP_GEN_ARITH_BINARY, //4F
  OP_GEN_STACK, OP_GEN_STACK, OP_GEN_STACK, OP_GEN_STACK, //53
  OP_GEN_STACK, OP_GEN_STACK, OP_GEN_STACK, OP_GEN_STACK, //57
  OP_GEN_STACK, OP_GEN_STACK, OP_GEN_STACK, OP_GEN_STACK, //5B
  OP_GEN_STACK, OP_GEN_STACK, OP_GEN_STACK, OP_GEN_STACK, //5F
  OP_GEN_STACK, OP_GEN_STACK, OP_GEN_BREAK, OP_GEN_CONVERSION, //63
  OP_PREFIX_SEGREG, OP_PREFIX_SEGREG, OP_PREFIX, OP_PREFIX, //67
  OP_GEN_STACK, OP_GEN_ARITH_BINARY, OP_GEN_STACK, OP_GEN_ARITH_BINARY, //6B
  OP_GEN_INOUT, OP_GEN_INOUT, OP_GEN_INOUT, OP_GEN_INOUT, //6F
  OP_GEN_BRANCH_COND, OP_GEN_BRANCH_COND, OP_GEN_BRANCH_COND, OP_GEN_BRANCH_COND, //73
  OP_GEN_BRANCH_COND, OP_GEN_BRANCH_COND, OP_GEN_BRANCH_COND, OP_GEN_BRANCH_COND, //77
  OP_GEN_BRANCH_COND, OP_GEN_BRANCH_COND, OP_GEN_BRANCH_COND, OP_GEN_BRANCH_COND, //7B
  OP_GEN_BRANCH_COND, OP_GEN_BRANCH_COND, OP_GEN_BRANCH_COND, OP_GEN_BRANCH_COND, //7F
  OP_GEN_ARITH_BINARY, OP_GEN_ARITH_BINARY, OP_GEN_ARITH_BINARY, OP_GEN_ARITH_BINARY, //83
  OP_GEN_ARITH_BINARY, OP_GEN_ARITH_BINARY, OP_GEN_DATAMOV, OP_GEN_DATAMOV, //87
  OP_GEN_DATAMOV, OP_GEN_DATAMOV, OP_GEN_DATAMOV, OP_GEN_DATAMOV, //8B
  OP_GEN_DATAMOV, OP_GEN_DATAMOV, OP_GEN_DATAMOV, OP_GEN_STACK, //8F
  OP_GEN_DATAMOV, OP_GEN_DATAMOV, OP_GEN_DATAMOV, OP_GEN_DATAMOV, //93
  OP_GEN_DATAMOV, OP_GEN_DATAMOV, OP_GEN_DATAMOV, OP_GEN_DATAMOV, //97
  OP_GEN_CONVERSION, OP_GEN_CONVERSION, OP_GEN_BRANCH, OP_PREFIX_X87FPU, //9B
  OP_GEN_STACK, OP_GEN_STACK, OP_GEN_DATAMOV, OP_GEN_DATAMOV, //9F
  OP_GEN_DATAMOV, OP_GEN_DATAMOV, OP_GEN_DATAMOV, OP_GEN_DATAMOV, //A3
  OP_GEN_STRING, OP_GEN_STRING, OP_GEN_STRING, OP_GEN_STRING, //A7
  OP_GEN_LOGICAL, OP_GEN_LOGICAL, OP_GEN_STRING, OP_GEN_STRING, //AB
  OP_GEN_STRING, OP_GEN_STRING, OP_GEN_STRING, OP_GEN_STRING, //AF
  OP_GEN_DATAMOV, OP_GEN_DATAMOV, OP_GEN_DATAMOV, OP_GEN_DATAMOV, //B3
  OP_GEN_DATAMOV, OP_GEN_DATAMOV, OP_GEN_DATAMOV, OP_GEN_DATAMOV, //B7
  OP_GEN_DATAMOV, OP_GEN_DATAMOV, OP_GEN_DATAMOV, OP_GEN_DATAMOV, //BB
  OP_GEN_DATAMOV, OP_GEN_DATAMOV, OP_GEN_DATAMOV, OP_GEN_DATAMOV, //BF
  OP_GEN_SHF_ROT, OP_GEN_SHF_ROT, OP_GEN_BRANCH, OP_GEN_BRANCH, //C3
  OP_GEN_DATAMOV, OP_GEN_DATAMOV, OP_GEN_DATAMOV, OP_GEN_DATAMOV, //C7
  OP_GEN_STACK, OP_GEN_STACK, OP_GEN_BRANCH, OP_GEN_BRANCH, //CB
  OP_GEN_BREAK, OP_GEN_BREAK, OP_GEN_BREAK, OP_GEN_BREAK, //CF
  OP_GEN_SHF_ROT, OP_GEN_SHF_ROT, OP_GEN_SHF_ROT, OP_GEN_SHF_ROT, //D3
  OP_GEN_ARITH_DECIMAL, OP_GEN_ARITH_DECIMAL, OP_GEN_DATAMOV, OP_GEN_DATAMOV, //D7
  OP_X87_ARITH, OP_X87_DATAMOV, OP_X87_ARITH, OP_X87_DATAMOV, //DB
  OP_X87_ARITH, OP_X87_DATAMOV, OP_X87_ARITH, OP_X87_DATAMOV, //DF
  OP_GEN_BRANCH_COND, OP_GEN_BRANCH_COND, OP_GEN_BRANCH_COND, OP_GEN_BRANCH_COND, //E3
  OP_GEN_INOUT, OP_GEN_INOUT, OP_GEN_INOUT, OP_GEN_INOUT, //E7
  OP_GEN_BRANCH, OP_GEN_BRANCH, OP_GEN_BRANCH, OP_GEN_BRANCH, //EB
  OP_GEN_INOUT, OP_GEN_INOUT, OP_GEN_INOUT, OP_GEN_INOUT, //EF
  OP_PREFIX, OP_GEN_BREAK, OP_PREFIX, OP_PREFIX, //F3
  OP_SYSTEM, OP_GEN_FLAG_CONTROL, OP_GEN_ARITH_BINARY, OP_GEN_ARITH_BINARY, //F7
  OP_GEN_FLAG_CONTROL, OP_GEN_FLAG_CONTROL, OP_GEN_FLAG_CONTROL, OP_GEN_FLAG_CONTROL, //FB
  OP_GEN_FLAG_CONTROL, OP_GEN_FLAG_CONTROL, OP_GEN_ARITH_BINARY, OP_GEN_BRANCH, //FF
};

static const uint8_t typeOp2[256] = {
  OP_SYSTEM, OP_SYSTEM, OP_SYSTEM, OP_SYSTEM, //03
  OP_INVALID, OP_SYSTEM, OP_SYSTEM, OP_SYSTEM, //07
  OP_SYSTEM, OP_SYSTEM, OP_INVALID, OP_GEN_CONTROL, //0B
  OP_INVALID, OP_GEN_CONTROL, OP_INVALID, OP_INVALID, //0F
  OP_SSE_DATAMOV, OP_SSE_DATAMOV, OP_SSE_DATAMOV, OP_SSE_DATAMOV, //13
  OP_SSE, OP_SSE, OP_SSE_DATAMOV, OP_SSE_DATAMOV, //17
  OP_SSE, OP_GEN_CONTROL, OP_GEN_CONTROL, OP_GEN_CONTROL, //1B
  OP_GEN_CONTROL, OP_GEN_CONTROL, OP_GEN_CONTROL, OP_GEN_CONTROL, //1F
  OP_SYSTEM, OP_SYSTEM, OP_SYSTEM, OP_SYSTEM, //23
  OP_SYSTEM, OP_INVALID, OP_SYSTEM, OP_INVALID, //27
  OP_SSE_DATAMOV, OP_SSE_DATAMOV, OP_SSE, OP_SSE, //2B
  OP_SSE, OP_SSE, OP_SSE, OP_SSE, //2F
  OP_SYSTEM, OP_SYSTEM, OP_SYSTEM, OP_SYSTEM, //33
  OP_SYSTEM, OP_SYSTEM, OP_INVALID, OP_INVALID, //37
  OP_PREFIX, OP_INVALID, OP_PREFIX, OP_INVALID, //3B
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //3F
  OP_GEN_DATAMOV, OP_GEN_DATAMOV, OP_GEN_DATAMOV, OP_GEN_DATAMOV, //43
  OP_GEN_DATAMOV, OP_GEN_DATAMOV, OP_GEN_DATAMOV, OP_GEN_DATAMOV, //47
  OP_GEN_DATAMOV, OP_GEN_DATAMOV, OP_GEN_DATAMOV, OP_GEN_DATAMOV, //4B
  OP_GEN_DATAMOV, OP_GEN_DATAMOV, OP_GEN_DATAMOV, OP_GEN_DATAMOV, //4F
  OP_SSE_DATAMOV, OP_SSE, OP_SSE, OP_SSE, //53
  OP_SSE, OP_SSE, OP_SSE, OP_SSE, //57
  OP_SSE, OP_SSE, OP_SSE, OP_SSE, //5B
  OP_SSE, OP_SSE, OP_SSE, OP_SSE, //5F
  OP_MMX, OP_MMX, OP_MMX, OP_MMX, //63
  OP_MMX, OP_MMX, OP_MMX, OP_MMX, //67
  OP_MMX, OP_MMX, OP_MMX, OP_MMX, //6B
  OP_INVALID, OP_INVALID, OP_MMX, OP_MMX, //6F
  OP_SSE, OP_MMX, OP_MMX, OP_MMX, //73
  OP_MMX, OP_MMX, OP_MMX, OP_MMX, //77
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //7B
  OP_INVALID, OP_INVALID, OP_MMX, OP_MMX, //7F
  OP_GEN_BRANCH_COND, OP_GEN_BRANCH_COND, OP_GEN_BRANCH_COND, OP_GEN_BRANCH_COND, //83
  OP_GEN_BRANCH_COND, OP_GEN_BRANCH_COND, OP_GEN_BRANCH_COND, OP_GEN_BRANCH_COND, //87
  OP_GEN_BRANCH_COND, OP_GEN_BRANCH_COND, OP_GEN_BRANCH_COND, OP_GEN_BRANCH_COND, //8B
  OP_GEN_BRANCH_COND, OP_GEN_BRANCH_COND, OP_GEN_BRANCH_COND, OP_GEN_BRANCH_COND, //8F
  OP_GEN_DATAMOV, OP_GEN_DATAMOV, OP_GEN_DATAMOV, OP_GEN_DATAMOV, //93
  OP_GEN_DATAMOV, OP_GEN_DATAMOV, OP_GEN_DATAMOV, OP_GEN_DATAMOV, //97
  OP_GEN_DATAMOV, OP_GEN_DATAMOV, OP_GEN_DATAMOV, OP_GEN_DATAMOV, //9B
  OP_GEN_DATAMOV, OP_GEN_DATAMOV, OP_GEN_DATAMOV, OP_GEN_DATAMOV, //9F
  OP_GEN_STACK, OP_GEN_STACK, OP_GEN_CONTROL, OP_GEN_BIT, //A3
  OP_GEN_SHF_ROT, OP_GEN_SHF_ROT, OP_INVALID, OP_INVALID, //A7
  OP_GEN_STACK, OP_GEN_STACK, OP_SYSTEM, OP_GEN_BIT, //AB
  OP_GEN_SHF_ROT, OP_GEN_SHF_ROT, OP_STATE_MANAGEMENT, OP_GEN_ARITH_BINARY, //AF
  OP_GEN_DATAMOV, OP_GEN_DATAMOV, OP_GEN_DATAMOV, OP_GEN_BIT, //B3
  OP_GEN_DATAMOV, OP_GEN_DATAMOV, OP_GEN_CONVERSION, OP_GEN_CONVERSION, //B7
  OP_INVALID, OP_GEN_CONTROL, OP_GEN_BIT, OP_GEN_BIT, //BB
  OP_GEN_BIT, OP_GEN_BIT, OP_GEN_CONVERSION, OP_GEN_CONVERSION, //BF
  OP_GEN_DATAMOV, OP_GEN_DATAMOV, OP_SSE, OP_SSE, //C3
  OP_SSE, OP_SSE, OP_SSE, OP_GEN_DATAMOV, //C7
  OP_GEN_DATAMOV, OP_GEN_DATAMOV, OP_GEN_DATAMOV, OP_GEN_DATAMOV, //CB
  OP_GEN_DATAMOV, OP_GEN_DATAMOV, OP_GEN_DATAMOV, OP_GEN_DATAMOV, //CF
  OP_INVALID, OP_MMX, OP_MMX, OP_MMX, //D3
  OP_SSE, OP_MMX, OP_INVALID, OP_SSE, //D7
  OP_MMX, OP_MMX, OP_SSE, OP_MMX, //DB
  OP_MMX, OP_MMX, OP_SSE, OP_MMX, //DF
  OP_SSE, OP_MMX, OP_SSE, OP_MMX, //E3
  OP_SSE, OP_MMX, OP_INVALID, OP_SSE, //E7
  OP_MMX, OP_MMX, OP_SSE, OP_MMX, //EB
  OP_MMX, OP_MMX, OP_SSE, OP_MMX, //EF
  OP_INVALID, OP_MMX, OP_MMX, OP_MMX, //F3
  OP_SSE, OP_MMX, OP_SSE, OP_SSE, //F7
  OP_MMX, OP_MMX, OP_MMX, OP_SSE, //FB
  OP_MMX, OP_MMX, OP_MMX, OP_INVALID, //FF
};

static const uint8_t typeOp3_38[256] = {
  OP_SSE, OP_SSE, OP_SSE, OP_SSE, //03
  OP_SSE, OP_SSE, OP_SSE, OP_SSE, //07
  OP_SSE, OP_SSE, OP_SSE, OP_SSE, //0B
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //0F
  OP_SSE, OP_INVALID, OP_INVALID, OP_INVALID, //13
  OP_SSE, OP_SSE, OP_INVALID, OP_SSE, //17
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //1B
  OP_SSE, OP_SSE, OP_SSE, OP_INVALID, //1F
  OP_SSE, OP_SSE, OP_SSE, OP_SSE, //23
  OP_SSE, OP_SSE, OP_INVALID, OP_INVALID, //27
  OP_SSE, OP_SSE, OP_SSE, OP_SSE, //2B
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //2F
  OP_SSE, OP_SSE, OP_SSE, OP_SSE, //33
  OP_SSE, OP_SSE, OP_INVALID, OP_SSE, //37
  OP_SSE, OP_SSE, OP_SSE, OP_SSE, //3B
  OP_SSE, OP_SSE, OP_SSE, OP_SSE, //3F
  OP_SSE, OP_SSE, OP_INVALID, OP_INVALID, //43
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //47
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //4B
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //4F
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //53
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //57
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //5B
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //5F
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //63
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //67
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //6B
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //6F
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //73
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //77
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //7B
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //7F
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //83
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //87
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //8B
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //8F
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //93
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //97
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //9B
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //9F
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //A3
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //A7
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //AB
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //AF
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //B3
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //B7
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //BB
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //BF
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //C3
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //C7
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //CB
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //CF
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //D3
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //D7
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //DB
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //DF
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //E3
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //E7
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //EB
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //EF
  OP_GEN_DATAMOV, OP_GEN_DATAMOV, OP_INVALID, OP_INVALID, //F3
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //F7
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //FB
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //FF
};

static const uint8_t typeOp3_3A[256] = {
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //03
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //07
  OP_SSE, OP_SSE, OP_SSE, OP_SSE, //0B
  OP_SSE, OP_SSE, OP_SSE, OP_SSE, //0F
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //13
  OP_SSE, OP_SSE, OP_SSE, OP_SSE, //17
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //1B
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //1F
  OP_SSE, OP_SSE, OP_SSE, OP_INVALID, //23
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //27
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //2B
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //2F
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //33
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //37
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //3B
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //3F
  OP_SSE, OP_SSE, OP_SSE, OP_INVALID, //43
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //47
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //4B
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //4F
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //53
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //57
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //5B
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //5F
  OP_SSE, OP_SSE, OP_SSE, OP_SSE, //63
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //67
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //6B
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //6F
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //73
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //77
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //7B
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //7F
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //83
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //87
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //8B
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //8F
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //93
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //97
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //9B
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //9F
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //A3
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //A7
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //AB
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //AF
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //B3
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //B7
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //BB
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //BF
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //C3
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //C7
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //CB
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //CF
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //D3
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //D7
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //DB
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //DF
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //E3
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //E7
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //EB
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //EF
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //F3
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //F7
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //FB
  OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //FF
};

static const uint8_t typeOpX[32] = {
  // escapes for F6
  OP_GEN_LOGICAL, OP_GEN_LOGICAL, OP_GEN_LOGICAL, OP_GEN_ARITH_BINARY, OP_GEN_ARITH_BINARY, OP_GEN_ARITH_BINARY,
  OP_GEN_ARITH_BINARY, OP_GEN_ARITH_BINARY,
  // escapes for F7
  OP_GEN_LOGICAL, OP_GEN_LOGICAL, OP_GEN_LOGICAL, OP_GEN_ARITH_BINARY, OP_GEN_ARITH_BINARY, OP_GEN_ARITH_BINARY,
  OP_GEN_ARITH_BINARY, OP_GEN_ARITH_BINARY,
  // escapes for FE
  OP_GEN_ARITH_BINARY, OP_GEN_ARITH_BINARY, OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID,
  // escapes for FF
  OP_GEN_ARITH_BINARY, OP_GEN_ARITH_BINARY, OP_GEN_BRANCH, OP_GEN_BRANCH, OP_GEN_BRANCH, OP_GEN_BRANCH, OP_GEN_STACK,
  OP_INVALID,
};

enum Prefixes {
  ES_OVERRIDE = 0x26,
  CS_OVERRIDE = 0x2E,
  SS_OVERRIDE = 0x36,
  DS_OVERRIDE = 0x3E,
  FS_OVERRIDE = 0x64,
  GS_OVERRIDE = 0x65,
  AD_OVERRIDE = 0x67,
  WAIT_FPU = 0x9B,
  LOCK = 0xF0,
  REP_N_STR = 0xF2,
  REP_STR = 0xF3,
};

/**
  * 1-byte opcodes of special interest (for one reason or another)
  */
enum Opcodes {
  OP_2BYTE = 0x0f, /**< start of 2-byte opcode */
  OP_OSIZE = 0x66, /**< operand size prefix */
  OP_CALLF = 0x9a, OP_RETNI = 0xc2, /**< ret near+immediate */
  OP_RETN = 0xc3, OP_ENTER = 0xc8, OP_INT3 = 0xcc, OP_INTO = 0xce, OP_CALLN = 0xe8, OP_JMPF = 0xea, OP_ICEBP = 0xf1,
};

typedef enum ExeState {
  Start = 0,
  PrefOpSize = 1,
  PrefMultiByteOp = 2,
  ParseFlags = 3,
  ExtraFlags = 4,
  ReadModRM = 5,
  Read_OP3_38 = 6,
  Read_OP3_3A = 7,
  ReadSIB = 8,
  Read8 = 9,
  Read16 = 10,
  Read32 = 11,
  Read8ModRm = 12,
  Read16F = 13,
  Read32ModRm = 14,
  Error = 15,
} ExeState;

#define CacheSize 32u

typedef struct OpCache {
    uint32_t Op[CacheSize];
    uint32_t Index;
} OpCache;

typedef struct Instruction {
    uint32_t data;
    uint8_t prefix, code, ModRM, SIB, REX, flags, bytesRead, size, category;
    bool mustCheckRex, decoding, o16, imm8;
} Instruction;

#define codeShift (3)
#define codeMask ((uint32_t)(0xFF << codeShift)) /**< 0x000007F8 */
#define clearCodeMask ((uint32_t)(0xFFFFFFFF ^ codeMask)) /**< 0xFFFFF807 */
#define prefixMask ((uint32_t)((1 << codeShift) - 1)) /**< 0x07 */
#define operandSizeOverride ((uint32_t)(0x01 << (8 + codeShift))) /**< 0x00000800 */
#define multiByteOpcode ((uint32_t)(0x02 << (8 + codeShift))) /**< 0x00001000 */
#define prefixRex ((uint32_t)(0x04 << (8 + codeShift))) /**< 0x00002000 */
#define prefix38 ((uint32_t)(0x08 << (8 + codeShift))) /**< 0x00004000 */
#define prefix3A ((uint32_t)(0x10 << (8 + codeShift))) /**< 0x00008000 */
#define hasExtraFlags ((uint32_t)(0x20 << (8 + codeShift))) /**< 0x00010000 */
#define hasModRm ((uint32_t)(0x40 << (8 + codeShift))) /**< 0x00020000 */
#define ModRMShift (7 + 8 + codeShift) /**< 18 */
#define SIBScaleShift (ModRMShift + 8 - 6) /**< 20 */
#define regDWordDisplacement ((uint32_t)(1 << (8 + SIBScaleShift))) /**< 0x10000000 */
#define addressMode ((uint32_t)(2 << (8 + SIBScaleShift))) /**< 0x20000000 */
#define typeShift (2 + 8 + SIBScaleShift) /**< 30 */
#define categoryShift ((uint32_t)(5))
#define categoryMask ((uint32_t)(((1 << categoryShift) - 1))) /**< 0x1F (31) */
#define ModRM_mod ((uint8_t)0xC0)
#define ModRM_reg ((uint8_t)0x38)
#define ModRM_rm ((uint8_t)0x07)
#define SIB_scale ((uint8_t)0xC0)
#define SIB_index ((uint8_t)0x38) /**< unused */
#define SIB_base ((uint8_t)0x07)
#define REX_w ((uint8_t)0x08)
#define minRequired ((uint32_t)(8)) /**< minimum required consecutive valid instructions to be considered as code */

struct ExeModel {
  Shared* shared;
  ContextMap2 cm;
  IndirectMap iMap;
  OpCache cache;
  uint32_t stateBh[256];
  ExeState pState, state;
  Instruction op;
  uint32_t totalOps, opMask, opCategoryMask, context;
  uint64_t brkCtx; /**< hash */
  bool valid;
};

#define nCM1 ExeModel_nCM1

ExeModel* ExeModel_new(Shared* const sh, const uint64_t size) {
  ExeModel* const em = (ExeModel*)calloc(1, sizeof(ExeModel)); // cache, op, stateBh: all 0
  if (em == NULL) {
    quit("Out of memory.");
  }
  em->shared = sh;
  ContextMap2_init(&em->cm, sh, size, ExeModel_nCM1 + ExeModel_nCM2, 64);
  IndirectMap_init(&em->iMap, sh, 20, 1, 64, 1023);
  em->pState = Start;
  em->state = Start;
  em->totalOps = 0;
  em->opMask = 0;
  em->opCategoryMask = 0;
  em->context = 0;
  em->brkCtx = 0;
  em->valid = false;
  assert(isPowerOf2(size));
  return em;
}

void ExeModel_delete(ExeModel* const em) {
  if (em == NULL) {
    return;
  }
  ContextMap2_destroy(&em->cm);
  IndirectMap_destroy(&em->iMap);
  free(em);
}

static inline bool isInvalidX64Op(const uint8_t op) {
  for( int i = 0; i < 19; i++ ) {
    if( op == invalidX64Ops[i] ) {
      return true;
    }
  }
  return false;
}

static inline bool isValidX64Prefix(const uint8_t prefix) {
  for( int i = 0; i < 8; i++ ) {
    if( prefix == x64Prefixes[i] ) {
      return true;
    }
  }
  return ((prefix >= 0x40 && prefix <= 0x4F) || (prefix >= 0x64 && prefix <= 0x67));
}

static void processMode(Instruction* const op, ExeState* const state) {
  if((op->flags & fMODE) == fAM ) {
    op->data |= addressMode;
    op->bytesRead = 0;
    switch( op->flags & fTYPE ) {
      case fDR:
        op->data |= (2u << typeShift);
        // fall through
      case fDA:
        op->data |= (1u << typeShift);
        // fall through
      case fAD: {
        *state = Read32;
        break;
      }
      case fBR: {
        op->data |= (2u << typeShift);
        *state = Read8;
      }
    }
  } else {
    switch( op->flags & fTYPE ) {
      case fBI:
        *state = Read8;
        break;
      case fWI: {
        *state = Read16;
        op->data |= (1u << typeShift);
        op->bytesRead = 0;
        break;
      }
      case fDI: {
        // x64 Move with 8byte immediate? [REX.W is set, opcodes 0xB8+r]
        op->imm8 = ((op->REX & REX_w) > 0 && (op->code & 0xF8) == 0xB8);
        if( !op->o16 || op->imm8 ) {
          *state = Read32;
          op->data |= (2u << typeShift);
        } else {
          *state = Read16;
          op->data |= (3u << typeShift);
        }
        op->bytesRead = 0;
        break;
      }
      default:
        *state = Start; /*no immediate*/
    }
  }
}

static void processFlags2(Instruction* const op, ExeState* const state) {
  //if arriving from state ExtraFlags, we've already read the ModRM byte
  if((op->flags & fMODE) == fMR && *state != ExtraFlags ) {
    *state = ReadModRM;
    return;
  }
  processMode(op, state);
}

static void processFlags(Instruction* const op, ExeState* const state) {
  if( op->code == OP_CALLF || op->code == OP_JMPF || op->code == OP_ENTER ) {
    op->bytesRead = 0;
    *state = Read16F;
    return; //must exit, ENTER has ModRM too
  }
  processFlags2(op, state);
}

static void checkFlags(Instruction* const op, ExeState* const state) {
  //must peek at ModRM byte to read the REG part, so we can know the opcode
  if( op->flags == fMEXTRA ) {
    *state = ExtraFlags;
  } else if( op->flags == fERR ) {
    memset(op, 0, sizeof(Instruction));
    *state = Error;
  } else {
    processFlags(op, state);
  }
}

static void readFlags(Instruction* const op, ExeState* const state) {
  op->flags = table1[op->code];
  op->category = typeOp1[op->code];
  checkFlags(op, state);
}

static void processModRm(Instruction* const op, ExeState* const state) {
  if((op->ModRM & ModRM_mod) == 0x40 ) {
    *state = Read8ModRm; //register+byte displacement
  } else if((op->ModRM & ModRM_mod) == 0x80 || (op->ModRM & (ModRM_mod | ModRM_rm)) == 0x05 ||
            (op->ModRM < 0x40 && (op->SIB & SIB_base) == 0x05)) {
    *state = Read32ModRm; //register+dword displacement
    op->bytesRead = 0;
  } else {
    processMode(op, state);
  }
}

static void applyCodeAndSetFlag(Instruction* const op, const uint32_t flag) {
  op->data &= clearCodeMask;
  op->data |= (op->code << codeShift) | flag;
}

static inline uint32_t opN(const OpCache* const cache, const uint32_t n) {
  return cache->Op[(cache->Index - n) & (CacheSize - 1)];
}

static inline uint32_t opNCategory(const uint32_t mask, const uint32_t n) {
  return ((mask >> (categoryShift * (n - 1))) & categoryMask);
}

static inline uint32_t pref(const ExeModel* const em, const uint32_t i) {
  const Shared* const shared = em->shared;
  INJECT_SHARED_buf
  return (uint32_t)(RingBuffer_back(buf, i) == 0x0f) + 2 * (uint32_t)(RingBuffer_back(buf, i) == 0x66) + 3 * (uint32_t)(RingBuffer_back(buf, i) == 0x67);
}

/**
  * Get context at buf(i) relevant to parsing 32-bit x86 code.
  * @param i
  * @param x
  * @return
  */
static uint32_t exeCxt(const ExeModel* const em, uint32_t i, uint32_t x) {
  const Shared* const shared = em->shared;
  INJECT_SHARED_buf
  uint32_t prefix = 0;
  uint32_t opcode = 0;
  uint32_t modRm = 0;
  uint32_t sib = 0;
  if( i != 0 ) {
    prefix += 4 * pref(em, i--);
  }
  if( i != 0 ) {
    prefix += pref(em, i--);
  }
  if( i != 0 ) {
    opcode += RingBuffer_back(buf, i--);
  }
  if( i != 0 ) {
    modRm += RingBuffer_back(buf, i--) & (ModRM_mod | ModRM_rm);
  }
  if((i != 0) && ((modRm & ModRM_rm) == 4) && (modRm < ModRM_mod)) {
    sib = RingBuffer_back(buf, i) & SIB_scale;
  }
  return prefix | opcode << 4 | modRm << 12 | x << 20 | sib << (28 - 6);
}

static void ExeModel_update(ExeModel* const em) {
  const Shared* const shared = em->shared;
  Instruction* const op = &em->op;
  OpCache* const cache = &em->cache;
  INJECT_SHARED_buf
  INJECT_SHARED_c1
  em->pState = em->state;
  op->size++;
  switch( em->state ) {
    case Start:
    case Error: {
      // previous code may have just been a REX prefix
      bool skip = false;
      if( op->mustCheckRex ) {
        op->mustCheckRex = false;
        // valid x64 code?
        if( !isInvalidX64Op(c1) && !isValidX64Prefix(c1)) {
          op->REX = op->code;
          op->code = c1;
          op->data = prefixRex | (op->code << codeShift) | (op->data & prefixMask);
          skip = true;
        }
      }

      op->ModRM = op->SIB = op->REX = op->flags = op->bytesRead = 0;
      if( !skip ) {
        op->code = c1;
        // possible REX prefix?
        op->mustCheckRex = ((op->code & 0xF0) == 0x40) && (!(op->decoding && ((op->data & prefixMask) == 1)));

        // check prefixes
        op->prefix = (int)(op->code == ES_OVERRIDE || op->code == CS_OVERRIDE || op->code == SS_OVERRIDE || op->code == DS_OVERRIDE) +
                    //invalid in x64
                    (int)(op->code == FS_OVERRIDE) * 2 + 
                    (int)(op->code == GS_OVERRIDE) * 3 +
                    (int)(op->code == AD_OVERRIDE) * 4 + 
                    (int)(op->code == WAIT_FPU) * 5 +
                    (int)(op->code == LOCK) * 6 + 
                    (int)(op->code == REP_N_STR || op->code == REP_STR) * 7;

        if( !op->decoding ) {
          em->totalOps += (int)(op->data != 0) - (int)((cache->Index != 0) && cache->Op[cache->Index & (CacheSize - 1)] != 0);
          em->opMask = (em->opMask << 1) | (uint32_t)(em->state != Error);
          em->opCategoryMask = (em->opCategoryMask << categoryShift) | (op->category);
          op->size = 0;

          cache->Op[cache->Index & (CacheSize - 1)] = op->data;
          cache->Index++;

          if( op->prefix == 0 ) {
            op->data = op->code << codeShift;
          } else {
            op->data = op->prefix;
            op->category = typeOp1[op->code];
            op->decoding = true;
            em->brkCtx = hash3(0, op->prefix, em->opCategoryMask & categoryMask);
            break;
          }
        } else {
          // we only have enough bits for one prefix, so the
          // instruction will be encoded with the last one
          if( op->prefix == 0 ) {
            op->data |= (op->code << codeShift);
            op->decoding = false;
          } else {
            op->data = op->prefix;
            op->category = typeOp1[op->code];
            em->brkCtx = hash3(1, op->prefix, em->opCategoryMask & categoryMask);
            break;
          }
        }
      }

      if((op->o16 = (op->code == OP_OSIZE))) {
        em->state = PrefOpSize;
      } else if( op->code == OP_2BYTE ) {
        em->state = PrefMultiByteOp;
      } else {
        readFlags(op, &em->state);
      }
      em->brkCtx = hash5(2, em->state, op->code, (em->opCategoryMask & categoryMask), opN(cache, 1) & ((ModRM_mod | ModRM_reg | ModRM_rm) << ModRMShift));
      break;
    }
    case PrefOpSize: {
      op->code = c1;
      applyCodeAndSetFlag(op, operandSizeOverride);
      readFlags(op, &em->state);
      em->brkCtx = hash2(3, em->state);
      break;
    }
    case PrefMultiByteOp: {
      op->code = c1;
      op->data |= multiByteOpcode;

      if( op->code == 0x38 ) {
        em->state = Read_OP3_38;
      } else if( op->code == 0x3A ) {
        em->state = Read_OP3_3A;
      } else {
        applyCodeAndSetFlag(op, 0);
        op->flags = table2[op->code];
        op->category = typeOp2[op->code];
        checkFlags(op, &em->state);
      }
      em->brkCtx = hash2(4, em->state);
      break;
    }
    case ParseFlags: {
      processFlags(op, &em->state);
      em->brkCtx = hash2(5, em->state);
      break;
    }
    case ExtraFlags:
    case ReadModRM: {
      op->ModRM = c1;
      op->data |= (op->ModRM << ModRMShift) | hasModRm;
      op->SIB = 0;
      if( op->flags == fMEXTRA ) {
        op->data |= hasExtraFlags;
        int i = ((op->ModRM >> 3) & 0x07) | ((op->code & 0x01) << 3) | ((op->code & 0x08) << 1);
        op->flags = tableX[i];
        op->category = typeOpX[i];
        if( op->flags == fERR ) {
          memset(op, 0, sizeof(Instruction));
          em->state = Error;
          em->brkCtx = hash2(6, em->state);
          break;
        }
        processFlags(op, &em->state);
        em->brkCtx = hash2(7, em->state);
        break;
      }

      if((op->ModRM & ModRM_rm) == 4 && op->ModRM < ModRM_mod ) {
        em->state = ReadSIB;
        em->brkCtx = hash2(8, em->state);
        break;
      }

      processModRm(op, &em->state);
      em->brkCtx = hash3(9, em->state, op->code);
      break;
    }
    case Read_OP3_38:
    case Read_OP3_3A: {
      op->code = c1;
      applyCodeAndSetFlag(op, prefix38 << (em->state - Read_OP3_38));
      if( em->state == Read_OP3_38 ) {
        op->flags = table3_38[op->code];
        op->category = typeOp3_38[op->code];
      } else {
        op->flags = table3_3A[op->code];
        op->category = typeOp3_3A[op->code];
      }
      checkFlags(op, &em->state);
      em->brkCtx = hash2(10, em->state);
      break;
    }
    case ReadSIB: {
      op->SIB = c1;
      op->data |= ((op->SIB & SIB_scale) << SIBScaleShift);
      processModRm(op, &em->state);
      em->brkCtx = hash3(11, em->state, op->SIB & SIB_scale);
      break;
    }
    case Read8:
    case Read16:
    case Read32: {
      if( ++op->bytesRead >= ((em->state - Read8) << (int)((int)(op->imm8) + 1))) {
        op->bytesRead = 0;
        op->imm8 = false;
        em->state = Start;
      }
      em->brkCtx = hash5(12, em->state, op->flags & fMODE, op->bytesRead,
                    ((op->bytesRead > 1) ? (RingBuffer_back(buf, op->bytesRead) << 8) : 0) | ((op->bytesRead) != 0 ? c1 : 0));
      break;
    }
    case Read8ModRm: {
      processMode(op, &em->state);
      em->brkCtx = hash2(13, em->state);
      break;
    }
    case Read16F: {
      if( ++op->bytesRead == 2 ) {
        op->bytesRead = 0;
        processFlags2(op, &em->state);
      }
      em->brkCtx = hash2(14, em->state);
      break;
    }
    case Read32ModRm: {
      op->data |= regDWordDisplacement;
      if( ++op->bytesRead == 4 ) {
        op->bytesRead = 0;
        processMode(op, &em->state);
      }
      em->brkCtx = hash2(15, em->state);
      break;
    }
  }
  const ExeState state = em->state;
  em->valid = (em->totalOps > 2 * minRequired) && ((em->opMask & ((1u << minRequired) - 1)) == ((1u << minRequired) - 1));
  em->context = state + 16 * op->bytesRead + 16 * (op->REX & REX_w);
  em->stateBh[em->context] = (em->stateBh[em->context] << 8) | c1;

  bool forced = shared->State.blockType == BLOCKTYPE_EXE;
  if( em->valid || forced ) {
    ContextMap2* const cm = &em->cm;
    uint32_t mask = 0;
    uint32_t count0 = 0;
    uint32_t i = 0;
    while( i < nCM1 ) {
      if( i > 1 ) {
        mask = mask * 2 + (uint32_t)(RingBuffer_back(buf, i - 1) == 0);
        count0 += mask & 1;
      }
      int j = (i < 4) ? i + 1 : 5 + (i - 4) * (2 + (int)(i > 6));
      INJECT_SHARED_blockPos
      ContextMap2_set(cm, hash4(i, exeCxt(em, j, 
        RingBuffer_back(buf, 1) * (int)(j > 6)),
        ((1u << nCM1) | mask) * (uint32_t)(count0 * nCM1 / 2 >= i), 
        (0x08 | (blockPos & 0x07)) * (uint32_t)(i < 4)
      ));
      i++;
    }

    ContextMap2_set(cm, em->brkCtx);

    mask = prefixMask | (0xF8 << codeShift) | multiByteOpcode | prefix38 | prefix3A;
    ContextMap2_set(cm, hash6(++i, opN(cache, 1) & (mask | regDWordDisplacement | addressMode),
      state + 16 * op->bytesRead, op->data & mask, op->REX, op->category
    ));

    mask = 0x04 | (0xFE << codeShift) | multiByteOpcode | prefix38 | prefix3A | ((ModRM_mod | ModRM_reg) << ModRMShift);
    ContextMap2_set(cm, hash6(++i, opN(cache, 1) & mask, opN(cache, 2) & mask, opN(cache, 3) & mask,
      em->context + 256 * (int)((op->ModRM & ModRM_mod) == ModRM_mod),
      op->data & ((mask | prefixRex) ^ (ModRM_mod << ModRMShift))
    ));

    mask = 0x04 | codeMask;
    ContextMap2_set(cm, hash6(++i,
      opN(cache, 1) & mask, 
      opN(cache, 2) & mask, 
      opN(cache, 3) & mask, 
      opN(cache, 4) & mask,
      (op->data & mask) | ((uint32_t)(state) << 11) | (op->bytesRead << 15)
    ));

    mask = 0x04 | (0xFC << codeShift) | multiByteOpcode | prefix38 | prefix3A;
    ContextMap2_set(cm, hash6(++i, state + 16 * op->bytesRead, op->data & mask, op->category * 8 + (em->opMask & 0x07), op->flags,
                (int)((op->SIB & SIB_base) == 5) * 4 + (int)((op->ModRM & ModRM_reg) == ModRM_reg) * 2 +
                (int)((op->ModRM & ModRM_mod) == 0)
    ));
    mask = prefixMask | codeMask | operandSizeOverride | multiByteOpcode | prefixRex | prefix38 | prefix3A | hasExtraFlags | hasModRm |
           ((ModRM_mod | ModRM_rm) << ModRMShift);
    ContextMap2_set(cm, hash4(++i, op->data & mask, state + 16 * op->bytesRead, op->flags));

    mask = prefixMask | codeMask | operandSizeOverride | multiByteOpcode | prefix38 | prefix3A | hasExtraFlags | hasModRm;
    ContextMap2_set(cm, hash5(++i, opN(cache, 1) & mask, state,
                op->bytesRead * 2 + (int)((op->REX & REX_w) > 0),
                op->data & ((uint16_t)(mask ^ operandSizeOverride))
    ));
    mask = 0x04 | (0xFE << codeShift) | multiByteOpcode | prefix38 | prefix3A | (ModRM_reg << ModRMShift);
    ContextMap2_set(cm, hash5(++i,
      opN(cache, 1) & mask, 
      opN(cache, 2) & mask, 
      state + 16 * op->bytesRead, 
      op->data & (mask | prefixMask | codeMask)
    ));

    ContextMap2_set(cm, hash2(++i, state + 16 * op->bytesRead));

    ContextMap2_set(cm, hash4(++i,
      (0x100 | c1) * (uint32_t)(op->bytesRead > 0), 
      state + 16 * em->pState + 256 * op->bytesRead,
         (int)((op->flags & fMODE) == fAM) * 16 + 
                          (op->REX & REX_w) + 
         (int)((op->o16)) * 4 +
         (int)((op->code & 0xFE) == 0xE8) * 2 +
         (int)((op->data & multiByteOpcode) != 0 && (op->code & 0xF0) == 0x80)
    ));

    em->shared->State.x86_64.state = 0x80 | ((uint8_t)(state) << 3) | op->bytesRead;
  }
  else
    em->shared->State.x86_64.state = 0;
}

void ExeModel_mix(ExeModel* const em, Mixer* const m) {
  const Shared* const shared = em->shared;
  bool forced = shared->State.blockType == BLOCKTYPE_EXE;
  INJECT_SHARED_bpos
  if( bpos == 0 ) {
    ExeModel_update(em);
  }

  if( em->valid || forced ) {
    if (bpos == 0) {
      ContextMap2_setScale(&em->cm, forced ? 128 : 64); // +100% boost
      IndirectMap_setScale(&em->iMap, forced ? 128 : 64); // +100% boost
    }
    ContextMap2_mix(&em->cm, m);
    IndirectMap_set(&em->iMap, hash2(em->brkCtx, bpos));
    IndirectMap_mix(&em->iMap, m);
    } else {
      // no need to add inputs as exemodel is always the last model
    }

    const Instruction* const op = &em->op;
    const ExeState state = em->state;
    const uint32_t context = em->context;
    INJECT_SHARED_c0
    uint8_t s = (
      (em->stateBh[context] >> (28 - bpos)) & 0x08) |
      ((em->stateBh[context] >> (21 - bpos)) & 0x04) |
      ((em->stateBh[context] >> (14 - bpos)) & 0x02) |
      ((em->stateBh[context] >> (7 - bpos)) & 0x01) |
      ((uint32_t)(op->category == OP_GEN_BRANCH) << 4) |
      ((uint32_t)((c0 & ((1u << bpos) - 1)) == 0) << 5
    );

    Mixer_set(m, context * 4 + (s >> 4), 1024);
    Mixer_set(m, state * 64 + bpos * 8 + (uint32_t)(op->bytesRead > 0) * 4 + (s >> 4), 1024);
    Mixer_set(m, (uint32_t)(em->brkCtx & 0x1FF) | ((s & 0x20) << 4), 1024);
    Mixer_set(m, finalize64(hash3(op->code, state, opN(&em->cache, 1) & codeMask), 13), 8192);
    Mixer_set(m, finalize64(hash4(state, bpos, op->code, op->bytesRead), 13), 8192);
    Mixer_set(m, finalize64(hash4(state, (bpos << 2) | (c0 & 3), em->opCategoryMask & categoryMask,
      ((uint32_t)(op->category == OP_GEN_BRANCH) << 2) |
      ((uint32_t)((op->flags & fMODE) == fAM) << 1) |
      (uint32_t)(op->bytesRead > 0)), 13), 8192);
}
