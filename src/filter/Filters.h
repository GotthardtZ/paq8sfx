#pragma once

#include "../Encoder.h"
#include "../Shared.h"
#include "Filter.h"

// Decompress or compare a file
void decompressFile(const Shared* shared, const char* filename, FMode fMode, Encoder* en);

#ifdef FULL
// Compress a file: store its size, then split it into blocks by type.
// For each block the block header (type, size, info) and the content are
// compressed; the content of an x86/x64 block is transformed first.
void compressfile(const Shared* shared, const char* filename, uint64_t fileSize, Encoder* en);
#endif
