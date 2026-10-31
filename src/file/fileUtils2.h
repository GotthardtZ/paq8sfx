#pragma once

#include "FileDisk.hpp"

/**
 * Verify that the specified file exists and is readable, determine file size.
 * Quits if the file is 2 GB or larger.
 * @todo Large file support
 */
static uint64_t getFileSize(const char *filename) {
  FileDisk f;
  f.open(filename, true);
  f.setEnd();
  const uint64_t fileSize = f.curPos();
  f.close();
  if((fileSize >> 31) != 0 ) {
    quit("Large files not supported.");
  }
  return fileSize;
}
