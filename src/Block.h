#pragma once

#include "Shared.hpp"
#include "Encoder.hpp"
#include "BlockType.hpp"

namespace Block {

#ifdef FULL
  void EncodeBlockHeader(Encoder* const encoder, BlockType blockType, uint64_t blockSize, int blockInfo);
#endif
  uint64_t DecodeBlockHeader(Encoder* const encoder);
#ifdef FULL
  void EncodeBlockSize(Encoder* const encoder, uint64_t blockSize);
#endif
  uint64_t DecodeBlockSize(Encoder* const encoder);
#ifdef FULL
  void EncodeBlockType(Encoder* const encoder, BlockType blocktype);
#endif
  BlockType DecodeBlockType(Encoder* const encoder);
#ifdef FULL
  void EncodeInfo(Encoder* const encoder, int info);
#endif
  int DecodeInfo(Encoder* const encoder);
};
