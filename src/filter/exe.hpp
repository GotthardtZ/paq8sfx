#pragma once

#include "../file/File.hpp"
#include "../Block.hpp"
#include "../Encoder.hpp"
#include "Filter.hpp"

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
class ExeFilter : public Filter {
private:
  constexpr static int block = 0x10000; /**< block size */
  int info{};
public:

  void setBegin(int info) {
    this->info = info;
  }

#ifdef FULL

  /**
   * Transforms size bytes of in, writes the result to out.
   * @todo Large file support
   */
  void encode(File *in, File *out, uint64_t size, int info, int &/*headerSize*/) override {
    Array<uint8_t> blk(block);

    // Transform
    for( uint64_t offset = 0; offset < size; offset += block ) {
      uint32_t size1 = min(uint32_t(size - offset), block);
      int bytesRead = static_cast<int>(in->blockRead(&blk[0], size1));
      if( bytesRead != static_cast<int>(size1)) {
        quit("encodeExe read error");
      }
      for( int i = bytesRead - 1; i >= 5; --i ) {
        if((blk[i - 4] == 0xe8 || blk[i - 4] == 0xe9 || (blk[i - 5] == 0x0f && (blk[i - 4] & 0xf0) == 0x80)) &&
            (blk[i] == 0 || blk[i] == 0xff)) {
          int a = (blk[i - 3] | blk[i - 2] << 8 | blk[i - 1] << 16 | blk[i] << 24) + static_cast<int>(offset + info) + i + 1;
          a <<= 7;
          a >>= 7;
          blk[i] = a >> 24;
          blk[i - 1] = a ^ 176;
          blk[i - 2] = (a >> 8) ^ 176;
          blk[i - 3] = (a >> 16) ^ 176;
        }
      }
      out->blockWrite(&blk[0], bytesRead);
    }
  }

#else

  // The stub only decodes.
  void encode(File* /*in*/, File* /*out*/, uint64_t /*size*/, int /*info*/, int& /*headerSize*/) override {
  }

#endif

  /**
   * Decompresses size bytes with the encoder and reverses the transform.
   * The result is written to out (FDECOMPRESS) or compared with out (FCOMPARE).
   * @todo Large file support
   * @param diffFound when comparing: set to the position of the first difference + 1
   * @return size
   */
  uint64_t decode(File */*in*/, File* out, FMode fMode, uint64_t size, uint64_t& diffFound) override {
    int offset = 6;
    int a = 0;
    uint8_t c[6];
    uint64_t begin = info;
    for( int i = 4; i >= 0; i-- ) {
      c[i] = encoder->decompressByte(encoder->predictorMain); // Fill queue
    }

    while( offset < static_cast<int>(size) + 6 ) {
      memmove(c + 1, c, 5);
      if( offset <= static_cast<int>(size)) {
        c[0] = encoder->decompressByte(encoder->predictorMain);
      }
      // E8E9 transform: E8/E9 xx xx xx 00/FF -> subtract location from x
      if((c[0] == 0x00 || c[0] == 0xFF) && (c[4] == 0xE8 || c[4] == 0xE9 || (c[5] == 0x0F && (c[4] & 0xF0) == 0x80)) &&
          (((offset - 1) ^ (offset - 6)) & -block) == 0 && offset <= static_cast<int>(size)) { // not crossing block boundary
        a = ((c[1] ^ 176) | (c[2] ^ 176) << 8 | (c[3] ^ 176) << 16 | c[0] << 24) - offset - static_cast<int>(begin);
        a <<= 7;
        a >>= 7;
        c[3] = a;
        c[2] = a >> 8;
        c[1] = a >> 16;
        c[0] = a >> 24;
      }
      if( fMode == FMode::FDECOMPRESS ) {
        out->putChar(c[5]);
      } else if( fMode == FMode::FCOMPARE && c[5] != out->getchar() && (diffFound == 0)) {
        diffFound = offset - 6 + 1;
      }
      if( fMode == FMode::FDECOMPRESS && ((offset & 0x0fff) == 0)) {
        encoder->printStatus();
      }
      offset++;
    }
    return size;
  }

};
