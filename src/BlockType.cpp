#include <cstdint>
#include "BlockType.hpp"

bool hasInfo(BlockType ft) {
  return ft == BlockType::EXE;
}

bool hasTransform(BlockType ft, int info) {
  return ft == BlockType::EXE;
}
