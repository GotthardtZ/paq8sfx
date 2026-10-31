#pragma once

#include <type_traits>

enum class BlockType {
  DEFAULT = 0,
  EXE,
  Count
};


bool hasInfo(BlockType ft);

bool hasTransform(BlockType ft, int info);
