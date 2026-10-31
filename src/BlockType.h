#pragma once

#include "SystemDefines.h"

typedef enum BlockType {
  BLOCKTYPE_DEFAULT = 0,
  BLOCKTYPE_EXE,
  BLOCKTYPE_COUNT
} BlockType;


bool hasInfo(BlockType ft);

bool hasTransform(BlockType ft, int info);
