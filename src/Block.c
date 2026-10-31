#include "Block.h"

#ifdef FULL
void Block_EncodeBlockHeader(Encoder* const encoder, BlockType blockType, uint64_t blockSize, int blockInfo) {
  Encoder_initContextForBlockModel(encoder, BLOCKTYPE_DEFAULT, 0);
  Block_EncodeBlockType(encoder, blockType);
  Block_EncodeBlockSize(encoder, blockSize);
  if (hasInfo(blockType))
    Block_EncodeInfo(encoder, blockInfo);
  Encoder_initContextForBlockModel(encoder, blockType, blockInfo);
}
#endif

uint64_t Block_DecodeBlockHeader(Encoder* const encoder) { //returns blockSize
  Encoder_initContextForBlockModel(encoder, BLOCKTYPE_DEFAULT, 0);
  BlockType blockType = Block_DecodeBlockType(encoder);
  uint64_t blockSize = Block_DecodeBlockSize(encoder);
  int blockInfo = -1;
  if (hasInfo(blockType))
    blockInfo = Block_DecodeInfo(encoder);
  Encoder_initContextForBlockModel(encoder, blockType, blockInfo);
  return blockSize;
}

#ifdef FULL
void Block_EncodeBlockType(Encoder* const encoder, BlockType blocktype) {
  Encoder_compressByte(encoder, encoder->predictorMain, (uint8_t)blocktype);
}
#endif

BlockType Block_DecodeBlockType(Encoder* const encoder) {
  BlockType blockType = (BlockType)Encoder_decompressByte(encoder, encoder->predictorMain);
  return blockType;
}

#ifdef FULL
void Block_EncodeBlockSize(Encoder* const encoder, uint64_t blockSize) {
  Encoder_compressByte(encoder, encoder->predictorMain, (blockSize >> 24) & 0xFF);
  Encoder_compressByte(encoder, encoder->predictorMain, (blockSize >> 16) & 0xFF);
  Encoder_compressByte(encoder, encoder->predictorMain, (blockSize >> 8) & 0xFF);
  Encoder_compressByte(encoder, encoder->predictorMain, (blockSize) & 0xFF);
}
#endif

uint64_t Block_DecodeBlockSize(Encoder* const encoder) {
  uint64_t blockSize = 0;
  uint8_t b;
  b = Encoder_decompressByte(encoder, encoder->predictorMain);
  blockSize = blockSize << 8 | b;
  b = Encoder_decompressByte(encoder, encoder->predictorMain);
  blockSize = blockSize << 8 | b;
  b = Encoder_decompressByte(encoder, encoder->predictorMain);
  blockSize = blockSize << 8 | b;
  b = Encoder_decompressByte(encoder, encoder->predictorMain);
  blockSize = blockSize << 8 | b;
  return blockSize;
}

#ifdef FULL
void Block_EncodeInfo(Encoder* const encoder, int info) {
  Encoder_compressByte(encoder, encoder->predictorMain, (info >> 24) & 0xFF);
  Encoder_compressByte(encoder, encoder->predictorMain, (info >> 16) & 0xFF);
  Encoder_compressByte(encoder, encoder->predictorMain, (info >> 8) & 0xFF);
  Encoder_compressByte(encoder, encoder->predictorMain, (info) & 0xFF);
}
#endif

int Block_DecodeInfo(Encoder* const encoder) {
  uint32_t info = 0;
  uint8_t b;
  b = Encoder_decompressByte(encoder, encoder->predictorMain);
  info = info << 8 | b;
  b = Encoder_decompressByte(encoder, encoder->predictorMain);
  info = info << 8 | b;
  b = Encoder_decompressByte(encoder, encoder->predictorMain);
  info = info << 8 | b;
  b = Encoder_decompressByte(encoder, encoder->predictorMain);
  info = info << 8 | b;
  return info;
}
