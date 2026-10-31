#pragma once

#include "FileDisk.h"

/**
 * Verify that the specified file exists and is readable, determine file size.
 * Quits if the file is 2 GB or larger.
 * @todo Large file support
 */
static inline uint64_t getFileSize(const char* const filename) {
  FileDisk f;
  FileDisk_init(&f);
  File_open(&f.base, filename, true);
  File_setEnd(&f.base);
  const uint64_t fileSize = File_curPos(&f.base);
  File_close(&f.base);
  if((fileSize >> 31) != 0 ) {
    quit("Large files not supported.");
  }
  return fileSize;
}
