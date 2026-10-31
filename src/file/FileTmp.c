#include "FileTmp.h"
#include "../Array.h"

// Makes contentInRam large enough for newSize bytes; the content is kept.
static void FileTmp_reserve(FileTmp* const self, const uint64_t newSize) {
  if (newSize <= self->reserved) {
    return;
  }
  uint64_t newReserved = self->reserved * 2 + 16;
  if (newReserved < newSize) {
    newReserved = newSize;
  }
  uint8_t* const newContent = (uint8_t*)Array_new(newReserved, 1, 16);
  if (self->fileSize > 0) {
    memcpy(newContent, self->contentInRam, (size_t)self->fileSize);
  }
  Array_delete(self->contentInRam);
  self->contentInRam = newContent;
  self->reserved = newReserved;
}

/**
  *  This method is forbidden for temporary files.
  */
static bool FileTmp_open(File* const f, const char* const filename, const bool mustSucceed) {
  (void)f; (void)filename; (void)mustSucceed;
  assert(false);
  return false;
}

/**
  *  This method is forbidden for temporary files.
  */
static void FileTmp_create(File* const f, const char* const filename) {
  (void)f; (void)filename;
  assert(false);
}

static void FileTmp_close(File* const f) {
  FileTmp* const self = (FileTmp*)f;
  Array_delete(self->contentInRam);
  self->contentInRam = NULL;
  self->reserved = 0;
  self->filePos = 0;
  self->fileSize = 0;
}

static int FileTmp_getChar(File* const f) {
  FileTmp* const self = (FileTmp*)f;
  if (self->filePos >= self->fileSize) {
    return EOF;
  }
  return self->contentInRam[self->filePos++];
}

static void FileTmp_putChar(File* const f, const uint8_t c) {
  FileTmp* const self = (FileTmp*)f;
  if (self->filePos == self->fileSize) {
    FileTmp_reserve(self, self->fileSize + 1);
    self->fileSize++;
  }
  self->contentInRam[self->filePos] = c;
  self->filePos++;
}

static uint64_t FileTmp_blockRead(File* const f, uint8_t* const ptr, uint64_t count) {
  FileTmp* const self = (FileTmp*)f;
  const uint64_t available = self->fileSize - self->filePos;
  if (available < count) {
    count = available;
  }
  if (count > 0) {
    memcpy(ptr, &self->contentInRam[self->filePos], (size_t)count);
    self->filePos += count;
  }
  return count;
}

static void FileTmp_blockWrite(File* const f, uint8_t* const ptr, const uint64_t count) {
  FileTmp* const self = (FileTmp*)f;
  FileTmp_reserve(self, self->filePos + count);
  if (count > 0) {
    memcpy(&self->contentInRam[self->filePos], ptr, (size_t)count);
  }
  self->filePos += count;
  if (self->filePos > self->fileSize) {
    self->fileSize = self->filePos;
  }
}

static void FileTmp_setpos(File* const f, const uint64_t newPos) {
  FileTmp* const self = (FileTmp*)f;
  assert(newPos <= self->fileSize);
  self->filePos = newPos;
}

static void FileTmp_setEnd(File* const f) {
  FileTmp* const self = (FileTmp*)f;
  self->filePos = self->fileSize;
}

static uint64_t FileTmp_curPos(File* const f) {
  return ((FileTmp*)f)->filePos;
}

static bool FileTmp_eof(File* const f) {
  FileTmp* const self = (FileTmp*)f;
  return self->filePos >= self->fileSize;
}

static const FileVtbl FileTmp_vtbl = {
  FileTmp_open, FileTmp_create, FileTmp_close, FileTmp_getChar, FileTmp_putChar,
  FileTmp_blockRead, FileTmp_blockWrite, FileTmp_setpos, FileTmp_setEnd, FileTmp_curPos, FileTmp_eof
};

void FileTmp_init(FileTmp* const f) {
  f->base.vt = &FileTmp_vtbl;
  f->contentInRam = NULL;
  f->reserved = 0;
  f->filePos = 0;
  f->fileSize = 0;
}
