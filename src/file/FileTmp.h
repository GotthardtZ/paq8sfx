#pragma once

#include "File.h"

/**
 * This class is responsible for temporary files.
 * All file operations use RAM exclusively.
 * FileTmp_init() makes an empty file; File_close() frees its memory.
 */
typedef struct FileTmp {
  File base;
  uint8_t* contentInRam; /**< content of file */
  uint64_t reserved; /**< size of contentInRam */
  uint64_t filePos;
  uint64_t fileSize;
} FileTmp;

void FileTmp_init(FileTmp* f);
