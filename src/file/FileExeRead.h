#pragma once

#include "../SystemDefines.h"

/**
 * The running executable file, loaded into memory.
 * The stub uses it to find the payload files that are attached to it.
 * Owns the buffer (malloc/free): FileExeRead_init() loads the file (or
 * quits), FileExeRead_destroy() frees it.
 */
typedef struct FileExeRead {
  uint8_t* buf;
  uint64_t fileSize;
} FileExeRead;

void FileExeRead_init(FileExeRead* self);
void FileExeRead_destroy(FileExeRead* self);
