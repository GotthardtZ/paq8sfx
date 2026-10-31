#pragma once

#include "File.h"
#include "fileUtils.h"

/**
 * This class is responsible for files on disk.
 * It simply passes function calls to stdio.
 * FileDisk_init() makes a closed file; close it with File_close().
 */
typedef struct FileDisk {
  File base;
  FILE* file;
} FileDisk;

void FileDisk_init(FileDisk* f);
