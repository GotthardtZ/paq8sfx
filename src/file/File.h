#pragma once

#include "../SystemDefines.h"

/**
 * This is the interface for all the required file operations.
 * FileDisk passes them on to the operating system, FileTmp keeps a temporary
 * file in RAM, and FileMemRead reads from a memory buffer.
 *
 * A File is the first member of every implementation; its function table
 * (vt) holds the implementation's functions. Call them through the File_*
 * functions below.
 */
typedef struct File File;

typedef struct FileVtbl {
  bool (*open)(File* f, const char* filename, bool mustSucceed);
  void (*create)(File* f, const char* filename);
  void (*close)(File* f);
  int (*getChar)(File* f);
  void (*putChar)(File* f, uint8_t c);
  uint64_t (*blockRead)(File* f, uint8_t* ptr, uint64_t count);
  void (*blockWrite)(File* f, uint8_t* ptr, uint64_t count);
  void (*setpos)(File* f, uint64_t newPos);
  void (*setEnd)(File* f);
  uint64_t (*curPos)(File* f);
  bool (*eof)(File* f);
} FileVtbl;

struct File {
  const FileVtbl* vt;
};

static inline bool File_open(File* const f, const char* const filename, const bool mustSucceed) { return f->vt->open(f, filename, mustSucceed); }
static inline void File_create(File* const f, const char* const filename) { f->vt->create(f, filename); }
static inline void File_close(File* const f) { f->vt->close(f); }
static inline int File_getChar(File* const f) { return f->vt->getChar(f); }
static inline void File_putChar(File* const f, const uint8_t c) { f->vt->putChar(f, c); }
static inline uint64_t File_blockRead(File* const f, uint8_t* const ptr, const uint64_t count) { return f->vt->blockRead(f, ptr, count); }
static inline void File_blockWrite(File* const f, uint8_t* const ptr, const uint64_t count) { f->vt->blockWrite(f, ptr, count); }
static inline void File_setpos(File* const f, const uint64_t newPos) { f->vt->setpos(f, newPos); }
static inline void File_setEnd(File* const f) { f->vt->setEnd(f); }
static inline uint64_t File_curPos(File* const f) { return f->vt->curPos(f); }
static inline bool File_eof(File* const f) { return f->vt->eof(f); }

void File_append(File* f, const char* s);
uint32_t File_get32(File* f);
void File_put32(File* f, uint32_t x);
uint64_t File_getVLI(File* f);
void File_putVLI(File* f, uint64_t i);
