#pragma once

#include "../file/File.h"
#include "../Block.h"
#include "../Encoder.h"
#include "Filter.h"

/**
 * EXE transform (E8/E9 transform) for x86/x64 code.
 *
 * A CALL or JMP instruction (E8/E9 xx xx xx 00/FF) and a conditional jump
 * (0F 8x xx xx xx 00/FF) hold a relative address, xxxxxxxx, LSB first. The
 * same target has a different relative address at every place it is called
 * from. The transform converts the relative address to an absolute one by
 * adding the position of the instruction, so that calls to the same target
 * look the same and compress better. Only addresses in the range +-2^24 are
 * handled; the arithmetic is done mod 2^25.
 *
 * The data is processed in blocks of 64 KB; instructions that cross a block
 * boundary are left as they are. "begin" is the position of the data in the
 * input file; it is stored with the block (as its block info), not in the
 * transformed data.
 */
typedef struct ExeFilter {
  Encoder* encoder;
  int info;
} ExeFilter;

void ExeFilter_init(ExeFilter* f);
void ExeFilter_setBegin(ExeFilter* f, int info);
void ExeFilter_setEncoder(ExeFilter* f, Encoder* en);

#ifdef FULL
/**
 * Transforms size bytes of in, writes the result to out.
 * @todo Large file support
 */
void ExeFilter_encode(ExeFilter* f, File* in, File* out, uint64_t size, int info, int* headerSize);
#endif

/**
 * Decompresses size bytes with the encoder and reverses the transform.
 * The result is written to out (FDECOMPRESS) or compared with out (FCOMPARE).
 * @todo Large file support
 * @param diffFound when comparing: set to the position of the first difference + 1
 * @return size
 */
uint64_t ExeFilter_decode(ExeFilter* f, File* in, File* out, FMode fMode, uint64_t size, uint64_t* diffFound);
