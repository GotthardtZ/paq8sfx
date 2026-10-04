#include "Block.hpp"

namespace Block {

  void EncodeBlockHeader(Encoder* const encoder, BlockType blockType, uint64_t blockSize, int blockInfo) {
    encoder->initContextForBlockModel(BlockType::DEFAULT, 0);
    EncodeBlockType(encoder, blockType);
    EncodeBlockSize(encoder, blockSize);
    if (hasInfo(blockType))
      EncodeInfo(encoder, blockInfo);
    encoder->initContextForBlockModel(blockType, blockInfo);
  }

  uint64_t DecodeBlockHeader(Encoder* const encoder) { //returns blockSize
    encoder->initContextForBlockModel(BlockType::DEFAULT, 0);
    BlockType blockType = DecodeBlockType(encoder);
    uint64_t blockSize = DecodeBlockSize(encoder);
    int blockInfo = -1;
    if (hasInfo(blockType))
      blockInfo = DecodeInfo(encoder);
    encoder->initContextForBlockModel(blockType, blockInfo);
    return blockSize;
  }

  void EncodeBlockType(Encoder* const encoder, BlockType blocktype) {
    encoder->compressByte(encoder->predictorMain, uint8_t(blocktype));
  }

  BlockType DecodeBlockType(Encoder* const encoder) {
    BlockType blockType = (BlockType)encoder->decompressByte(encoder->predictorMain);
    return blockType;
  }

  void EncodeBlockSize(Encoder* const encoder, uint64_t blockSize) {
    encoder->compressByte(encoder->predictorMain, (blockSize >> 24) & 0xFF);
    encoder->compressByte(encoder->predictorMain, (blockSize >> 16) & 0xFF);
    encoder->compressByte(encoder->predictorMain, (blockSize >> 8) & 0xFF);
    encoder->compressByte(encoder->predictorMain, (blockSize) & 0xFF);
  }

  uint64_t DecodeBlockSize(Encoder* const encoder) {
    uint64_t blockSize = 0;
    uint8_t b;
    b = encoder->decompressByte(encoder->predictorMain);
    blockSize = blockSize << 8 | b;
    b = encoder->decompressByte(encoder->predictorMain);
    blockSize = blockSize << 8 | b;
    b = encoder->decompressByte(encoder->predictorMain);
    blockSize = blockSize << 8 | b;
    b = encoder->decompressByte(encoder->predictorMain);
    blockSize = blockSize << 8 | b;
    return blockSize;
  }

  void EncodeInfo(Encoder* const encoder, int info) {
    encoder->compressByte(encoder->predictorMain, (info >> 24) & 0xFF);
    encoder->compressByte(encoder->predictorMain, (info >> 16) & 0xFF);
    encoder->compressByte(encoder->predictorMain, (info >> 8) & 0xFF);
    encoder->compressByte(encoder->predictorMain, (info) & 0xFF);
  }

  int DecodeInfo(Encoder* const encoder) {
    uint32_t info = 0;
    uint8_t b;
    b = encoder->decompressByte(encoder->predictorMain);
    info = info << 8 | b;
    b = encoder->decompressByte(encoder->predictorMain);
    info = info << 8 | b;
    b = encoder->decompressByte(encoder->predictorMain);
    info = info << 8 | b;
    b = encoder->decompressByte(encoder->predictorMain);
    info = info << 8 | b;
    return info;
  }


};
