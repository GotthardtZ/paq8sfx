#include "FileMemRead.h"

static bool FileMemRead_open(File* const f, const char* const filename, const bool mustSucceed) {
  (void)f; (void)filename; (void)mustSucceed;
  return true;
}

static void FileMemRead_create(File* const f, const char* const filename) { (void)f; (void)filename; }

static void FileMemRead_close(File* const f) { (void)f; }

static int FileMemRead_getChar(File* const f) {
  FileMemRead* const self = (FileMemRead*)f;
  return self->pos < self->size ? self->data[self->pos++] : EOF;
}

static void FileMemRead_putChar(File* const f, const uint8_t c) { (void)f; (void)c; }

static uint64_t FileMemRead_blockRead(File* const f, uint8_t* const ptr, const uint64_t count) {
  FileMemRead* const self = (FileMemRead*)f;
  const uint64_t n = count < self->size - self->pos ? count : self->size - self->pos;
  memcpy(ptr, self->data + self->pos, (size_t)n);
  self->pos += n;
  return n;
}

static void FileMemRead_blockWrite(File* const f, uint8_t* const ptr, const uint64_t count) { (void)f; (void)ptr; (void)count; }

static void FileMemRead_setpos(File* const f, const uint64_t newPos) {
  FileMemRead* const self = (FileMemRead*)f;
  self->pos = newPos < self->size ? newPos : self->size;
}

static void FileMemRead_setEnd(File* const f) {
  FileMemRead* const self = (FileMemRead*)f;
  self->pos = self->size;
}

static uint64_t FileMemRead_curPos(File* const f) { return ((FileMemRead*)f)->pos; }

static bool FileMemRead_eof(File* const f) {
  FileMemRead* const self = (FileMemRead*)f;
  return self->pos >= self->size;
}

static const FileVtbl FileMemRead_vtbl = {
  FileMemRead_open, FileMemRead_create, FileMemRead_close, FileMemRead_getChar, FileMemRead_putChar,
  FileMemRead_blockRead, FileMemRead_blockWrite, FileMemRead_setpos, FileMemRead_setEnd, FileMemRead_curPos, FileMemRead_eof
};

void FileMemRead_init(FileMemRead* const f, const uint8_t* const buffer, const uint64_t length) {
  f->base.vt = &FileMemRead_vtbl;
  f->data = buffer;
  f->size = length;
  f->pos = 0;
}
