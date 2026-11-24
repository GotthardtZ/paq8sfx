#include "../Array.h"
#include "ExeModel.h"
#include "../Hash.h"
#include "../Rle.h"

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

// The opcode tables, expanded by ExeModel_new(); their readable form is
// tools/tables/ExeModelTables.c.
#include "ExeModelTables.inc"

static uint8_t exeFlags[EXE_TABLES_SIZE];      /**< the format (fMR, ...) of each opcode */
static uint8_t exeCategories[EXE_TABLES_SIZE]; /**< the category (OP_...) of each opcode */

#define table1     (exeFlags + 0)         /**< 1 byte opcodes */
#define table2     (exeFlags + 256)       /**< 2 byte opcodes */
#define table3_38  (exeFlags + 512)       /**< 3 byte opcodes 0F38XX */
#define table3_3A  (exeFlags + 768)       /**< 3 byte opcodes 0F3AXX */
#define tableX     (exeFlags + 1024)      /**< escape opcodes using ModRM byte to get more variants */
#define typeOp1    (exeCategories + 0)
#define typeOp2    (exeCategories + 256)
#define typeOp3_38 (exeCategories + 512)
#define typeOp3_3A (exeCategories + 768)
#define typeOpX    (exeCategories + 1024)

static const uint8_t invalidX64Ops[19] = {0x06, 0x07, 0x16, 0x17, 0x1E, 0x1F, 0x27, 0x2F, 0x37, 0x3F, 0x60, 0x61, 0x62, 0x82, 0x9A, 0xD4, 0xD5, 0xD6, 0xEA,};
// The prefix bytes and their value in Instruction.prefix: segment overrides (1;
// FS 2 and GS 3 are invalid in x64), address size (4), WAIT (5), LOCK (6), REP (7)
static const uint8_t prefixes[11][2] = {
  { 0x26, 1 }, { 0x2E, 1 }, { 0x36, 1 }, { 0x3E, 1 }, { 0x64, 2 }, { 0x65, 3 },
  { 0x67, 4 }, { 0x9B, 5 }, { 0xF0, 6 }, { 0xF2, 7 }, { 0xF3, 7 },
};

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

// The small, often used members come first: they are reached with short
// (8-bit) offsets, the large arrays and sub-objects follow them.
struct ExeModel {
  Shared* shared;
  ExeState pState, state;
  Instruction op;
  uint32_t totalOps, opMask, opCategoryMask, context;
  uint64_t brkCtx; /**< hash */
  bool valid;
  OpCache cache;
  ContextMap2 cm;
  IndirectMap iMap;
  uint32_t stateBh[256];
};

#define nCM1 ExeModel_nCM1

ExeModel* ExeModel_new(Shared* const sh, const uint64_t size) {
  ExeModel* const em = ARRAY_NEW(ExeModel, 1); // zeroed
  Rle_decode(exeFlags, exeFlagsRle, EXE_TABLES_SIZE, 4);
  Rle_decode(exeCategories, exeCategoriesRle, EXE_TABLES_SIZE, 5);
  em->shared = sh;
  ContextMap2_init(&em->cm, sh, size, ExeModel_nCM1 + ExeModel_nCM2, 64);
  IndirectMap_init(&em->iMap, sh, 20, 1, 64, 1023);
  assert(isPowerOf2(size));
  return em;
}

static inline bool isInvalidX64Op(const uint8_t op) {
  for( int i = 0; i < 19; i++ ) {
    if( op == invalidX64Ops[i] ) {
      return true;
    }
  }
  return false;
}

// The value of a prefix byte in Instruction.prefix, 0 if it is not a prefix
static uint8_t prefixOf(const uint8_t c) {
  for (int i = 0; i < 11; i++) {
    if (c == prefixes[i][0]) {
      return prefixes[i][1];
    }
  }
  return 0;
}

static inline bool isValidX64Prefix(const uint8_t prefix) {
  // the prefixes above, REX (0x40..0x4F) and operand size (0x66)
  return prefixOf(prefix) != 0 || (prefix & 0xF0) == 0x40 || prefix == 0x66;
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

// hash6 of 6 values. Any hashN (N < 6) is the same as hash6 with the
// missing values set to UINT64_MAX: (UINT64_MAX + 1) * multiplier == 0.
static uint64_t ExeModel_hash6(const uint64_t* const x) {
  return hash6(x[0], x[1], x[2], x[3], x[4], x[5]);
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
  // brkCtx = hash6(bk[0], ..., bk[5]): bk[0] is a tag that tells which case
  // computed it, bk[1] is the new state (the prefix for tags 0 and 1); unused
  // values are UINT64_MAX, which do not change the hash (see ExeModel_hash6()).
  uint64_t bk[6] = { 0, 0, UINT64_MAX, UINT64_MAX, UINT64_MAX, UINT64_MAX };
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
        op->prefix = prefixOf(op->code);

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
            bk[0] = 0; bk[1] = op->prefix; bk[2] = em->opCategoryMask & categoryMask;
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
            bk[0] = 1; bk[1] = op->prefix; bk[2] = em->opCategoryMask & categoryMask;
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
      bk[0] = 2; bk[2] = op->code; bk[3] = em->opCategoryMask & categoryMask; bk[4] = opN(cache, 1) & ((ModRM_mod | ModRM_reg | ModRM_rm) << ModRMShift);
      break;
    }
    case PrefOpSize: {
      op->code = c1;
      applyCodeAndSetFlag(op, operandSizeOverride);
      readFlags(op, &em->state);
      bk[0] = 3;
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
      bk[0] = 4;
      break;
    }
    case ParseFlags: {
      processFlags(op, &em->state);
      bk[0] = 5;
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
          bk[0] = 6;
          break;
        }
        processFlags(op, &em->state);
        bk[0] = 7;
        break;
      }

      if((op->ModRM & ModRM_rm) == 4 && op->ModRM < ModRM_mod ) {
        em->state = ReadSIB;
        bk[0] = 8;
        break;
      }

      processModRm(op, &em->state);
      bk[0] = 9; bk[2] = op->code;
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
      bk[0] = 10;
      break;
    }
    case ReadSIB: {
      op->SIB = c1;
      op->data |= ((op->SIB & SIB_scale) << SIBScaleShift);
      processModRm(op, &em->state);
      bk[0] = 11; bk[2] = op->SIB & SIB_scale;
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
      bk[0] = 12; bk[2] = op->flags & fMODE; bk[3] = op->bytesRead;
      bk[4] = ((op->bytesRead > 1) ? (RingBuffer_back(buf, op->bytesRead) << 8) : 0) | ((op->bytesRead) != 0 ? c1 : 0);
      break;
    }
    case Read8ModRm: {
      processMode(op, &em->state);
      bk[0] = 13;
      break;
    }
    case Read16F: {
      if( ++op->bytesRead == 2 ) {
        op->bytesRead = 0;
        processFlags2(op, &em->state);
      }
      bk[0] = 14;
      break;
    }
    case Read32ModRm: {
      op->data |= regDWordDisplacement;
      if( ++op->bytesRead == 4 ) {
        op->bytesRead = 0;
        processMode(op, &em->state);
      }
      bk[0] = 15;
      break;
    }
  }
  if (bk[0] > 1) {
    bk[1] = em->state;
  }
  em->brkCtx = ExeModel_hash6(bk);
  const ExeState state = em->state;
  em->valid = (em->totalOps > 2 * minRequired) && ((em->opMask & ((1u << minRequired) - 1)) == ((1u << minRequired) - 1));
  em->context = state + 16 * op->bytesRead + 16 * (op->REX & REX_w);
  em->stateBh[em->context] = (em->stateBh[em->context] << 8) | c1;

  bool forced = shared->State.blockType == BLOCKTYPE_EXE;
  if( em->valid || forced ) {
    // The contexts, in order: row i is hashed as hash6(i, ...) with unused
    // values UINT64_MAX. Row nCM1 is brkCtx (already a hash).
    uint64_t cx[ExeModel_nCM1 + ExeModel_nCM2][6];
    memset(cx, 0xff, sizeof(cx));
    uint32_t mask = 0;
    uint32_t count0 = 0;
    INJECT_SHARED_blockPos
    for (uint32_t i = 0; i < nCM1; i++) {
      if( i > 1 ) {
        mask = mask * 2 + (uint32_t)(RingBuffer_back(buf, i - 1) == 0);
        count0 += mask & 1;
      }
      int j = (i < 4) ? i + 1 : 5 + (i - 4) * (2 + (int)(i > 6));
      cx[i][1] = exeCxt(em, j, RingBuffer_back(buf, 1) * (int)(j > 6));
      cx[i][2] = ((1u << nCM1) | mask) * (uint32_t)(count0 * nCM1 / 2 >= i);
      cx[i][3] = (0x08 | (blockPos & 0x07)) * (uint32_t)(i < 4);
    }

    uint64_t* r = cx[nCM1 + 1];
    mask = prefixMask | (0xF8 << codeShift) | multiByteOpcode | prefix38 | prefix3A;
    r[1] = opN(cache, 1) & (mask | regDWordDisplacement | addressMode);
    r[2] = state + 16 * op->bytesRead;
    r[3] = op->data & mask;
    r[4] = op->REX;
    r[5] = op->category;

    r += 6;
    mask = 0x04 | (0xFE << codeShift) | multiByteOpcode | prefix38 | prefix3A | ((ModRM_mod | ModRM_reg) << ModRMShift);
    r[1] = opN(cache, 1) & mask;
    r[2] = opN(cache, 2) & mask;
    r[3] = opN(cache, 3) & mask;
    r[4] = em->context + 256 * (int)((op->ModRM & ModRM_mod) == ModRM_mod);
    r[5] = op->data & ((mask | prefixRex) ^ (ModRM_mod << ModRMShift));

    r += 6;
    mask = 0x04 | codeMask;
    r[1] = opN(cache, 1) & mask;
    r[2] = opN(cache, 2) & mask;
    r[3] = opN(cache, 3) & mask;
    r[4] = opN(cache, 4) & mask;
    r[5] = (op->data & mask) | ((uint32_t)(state) << 11) | (op->bytesRead << 15);

    r += 6;
    mask = 0x04 | (0xFC << codeShift) | multiByteOpcode | prefix38 | prefix3A;
    r[1] = state + 16 * op->bytesRead;
    r[2] = op->data & mask;
    r[3] = op->category * 8 + (em->opMask & 0x07);
    r[4] = op->flags;
    r[5] = (int)((op->SIB & SIB_base) == 5) * 4 + (int)((op->ModRM & ModRM_reg) == ModRM_reg) * 2 +
           (int)((op->ModRM & ModRM_mod) == 0);

    r += 6;
    mask = prefixMask | codeMask | operandSizeOverride | multiByteOpcode | prefixRex | prefix38 | prefix3A | hasExtraFlags | hasModRm |
           ((ModRM_mod | ModRM_rm) << ModRMShift);
    r[1] = op->data & mask;
    r[2] = state + 16 * op->bytesRead;
    r[3] = op->flags;

    r += 6;
    mask = prefixMask | codeMask | operandSizeOverride | multiByteOpcode | prefix38 | prefix3A | hasExtraFlags | hasModRm;
    r[1] = opN(cache, 1) & mask;
    r[2] = state;
    r[3] = op->bytesRead * 2 + (int)((op->REX & REX_w) > 0);
    r[4] = op->data & ((uint16_t)(mask ^ operandSizeOverride));

    r += 6;
    mask = 0x04 | (0xFE << codeShift) | multiByteOpcode | prefix38 | prefix3A | (ModRM_reg << ModRMShift);
    r[1] = opN(cache, 1) & mask;
    r[2] = opN(cache, 2) & mask;
    r[3] = state + 16 * op->bytesRead;
    r[4] = op->data & (mask | prefixMask | codeMask);

    r += 6;
    r[1] = state + 16 * op->bytesRead;

    r += 6;
    r[1] = (0x100 | c1) * (uint32_t)(op->bytesRead > 0);
    r[2] = state + 16 * em->pState + 256 * op->bytesRead;
    r[3] = (int)((op->flags & fMODE) == fAM) * 16 +
           (op->REX & REX_w) +
           (int)((op->o16)) * 4 +
           (int)((op->code & 0xFE) == 0xE8) * 2 +
           (int)((op->data & multiByteOpcode) != 0 && (op->code & 0xF0) == 0x80);

    ContextMap2* const cm = &em->cm;
    for (uint32_t i = 0; i < ExeModel_nCM1 + ExeModel_nCM2; i++) {
      cx[i][0] = i;
      ContextMap2_set(cm, i == nCM1 ? em->brkCtx : ExeModel_hash6(cx[i]));
    }

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
