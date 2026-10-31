#include "BlockType.h"

bool hasInfo(BlockType ft) {
  return ft == BLOCKTYPE_EXE;
}

bool hasTransform(BlockType ft, int info) {
  (void)info;
  return ft == BLOCKTYPE_EXE;
}
