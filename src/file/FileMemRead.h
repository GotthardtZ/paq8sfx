#pragma once

#include "File.h"

/**
 * Read-only File over a memory buffer (which it does not own).
 * Used by the extract-only stub to decompress a payload straight from the
 * package image, without writing the compressed payload to disk first.
 * Write operations are ignored.
 */
typedef struct FileMemRead {
  File base;
  const uint8_t* data;
  uint64_t size;
  uint64_t pos;
} FileMemRead;

void FileMemRead_init(FileMemRead* f, const uint8_t* buffer, uint64_t length);
