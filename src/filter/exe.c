#include "exe.h"
#include "../Array.h"
#include "../Utils.h"

#define EXE_BLOCK 0x10000 /**< block size */

void ExeFilter_init(ExeFilter* const f) {
  f->encoder = NULL;
  f->info = 0;
}

void ExeFilter_setBegin(ExeFilter* const f, const int info) {
  f->info = info;
}

void ExeFilter_setEncoder(ExeFilter* const f, Encoder* const en) {
  f->encoder = en;
}

#ifdef FULL

void ExeFilter_encode(ExeFilter* const f, File* const in, File* const out, const uint64_t size, const int info, int* const headerSize) {
  (void)f; (void)headerSize;
  uint8_t* const blk = ARRAY_NEW(uint8_t, EXE_BLOCK);

  // Transform
  for( uint64_t offset = 0; offset < size; offset += EXE_BLOCK ) {
    uint32_t size1 = min((uint32_t)(size - offset), EXE_BLOCK);
    int bytesRead = (int)(File_blockRead(in, &blk[0], size1));
    if( bytesRead != (int)(size1)) {
      quit("encodeExe read error");
    }
    for( int i = bytesRead - 1; i >= 5; --i ) {
      if((blk[i - 4] == 0xe8 || blk[i - 4] == 0xe9 || (blk[i - 5] == 0x0f && (blk[i - 4] & 0xf0) == 0x80)) &&
          (blk[i] == 0 || blk[i] == 0xff)) {
        int a = (blk[i - 3] | blk[i - 2] << 8 | blk[i - 1] << 16 | blk[i] << 24) + (int)(offset + info) + i + 1;
        a <<= 7;
        a >>= 7;
        blk[i] = a >> 24;
        blk[i - 1] = a ^ 176;
        blk[i - 2] = (a >> 8) ^ 176;
        blk[i - 3] = (a >> 16) ^ 176;
      }
    }
    File_blockWrite(out, &blk[0], bytesRead);
  }
  Array_delete(blk);
}

#endif

uint64_t ExeFilter_decode(ExeFilter* const f, File* const in, File* const out, const FMode fMode, const uint64_t size, uint64_t* const diffFound) {
  (void)in;
  Encoder* const encoder = f->encoder;
  int offset = 6;
  int a = 0;
  uint8_t c[6];
  uint64_t begin = f->info;
  for( int i = 4; i >= 0; i-- ) {
    c[i] = Encoder_decompressByte(encoder, encoder->predictorMain); // Fill queue
  }

  while( offset < (int)(size) + 6 ) {
    memmove(c + 1, c, 5);
    if( offset <= (int)(size)) {
      c[0] = Encoder_decompressByte(encoder, encoder->predictorMain);
    }
    // E8E9 transform: E8/E9 xx xx xx 00/FF -> subtract location from x
    if((c[0] == 0x00 || c[0] == 0xFF) && (c[4] == 0xE8 || c[4] == 0xE9 || (c[5] == 0x0F && (c[4] & 0xF0) == 0x80)) &&
        (((offset - 1) ^ (offset - 6)) & -EXE_BLOCK) == 0 && offset <= (int)(size)) { // not crossing block boundary
      a = ((c[1] ^ 176) | (c[2] ^ 176) << 8 | (c[3] ^ 176) << 16 | c[0] << 24) - offset - (int)(begin);
      a <<= 7;
      a >>= 7;
      c[3] = a;
      c[2] = a >> 8;
      c[1] = a >> 16;
      c[0] = a >> 24;
    }
    if( fMode == FDECOMPRESS ) {
      File_putChar(out, c[5]);
    } else if( fMode == FCOMPARE && c[5] != File_getChar(out) && (*diffFound == 0)) {
      *diffFound = offset - 6 + 1;
    }
    if( fMode == FDECOMPRESS && ((offset & 0x0fff) == 0)) {
      Encoder_printStatus(encoder);
    }
    offset++;
  }
  return size;
}
