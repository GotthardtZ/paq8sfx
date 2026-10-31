#pragma once

#include "Shared.h"
#include "Encoder.h"
#include "BlockType.h"

#ifdef FULL
void Block_EncodeBlockHeader(Encoder* encoder, BlockType blockType, uint64_t blockSize, int blockInfo);
#endif
uint64_t Block_DecodeBlockHeader(Encoder* encoder);
#ifdef FULL
void Block_EncodeBlockSize(Encoder* encoder, uint64_t blockSize);
#endif
uint64_t Block_DecodeBlockSize(Encoder* encoder);
#ifdef FULL
void Block_EncodeBlockType(Encoder* encoder, BlockType blocktype);
#endif
BlockType Block_DecodeBlockType(Encoder* encoder);
#ifdef FULL
void Block_EncodeInfo(Encoder* encoder, int info);
#endif
int Block_DecodeInfo(Encoder* encoder);
