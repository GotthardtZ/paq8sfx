#include "FileDisk.h"

static bool FileDisk_open(File* const f, const char* const filename, const bool mustSucceed) {
  FileDisk* const self = (FileDisk*)f;
  assert(self->file == NULL);
  self->file = openFile(filename, FILE_READ);
  const bool success = (self->file != NULL);
  if( !success && mustSucceed ) {
    printf("Unable to open file %s (%s)", filename, strerror(errno));
    quit(NULL);
  }
  return success;
}

static void FileDisk_create(File* const f, const char* const filename) {
  FileDisk* const self = (FileDisk*)f;
  assert(self->file == NULL);
  makeDirectories(filename);
  self->file = openFile(filename, FILE_WRITE);
  if( self->file == NULL ) {
    printf("Unable to create file %s (%s)", filename, strerror(errno));
    quit(NULL);
  }
}

static void FileDisk_close(File* const f) {
  FileDisk* const self = (FileDisk*)f;
  if( self->file != NULL ) {
    fclose(self->file);
  }
  self->file = NULL;
}

static int FileDisk_getChar(File* const f) { return fgetc(((FileDisk*)f)->file); }

static void FileDisk_putChar(File* const f, const uint8_t c) { fputc(c, ((FileDisk*)f)->file); }

static uint64_t FileDisk_blockRead(File* const f, uint8_t* const ptr, const uint64_t count) { return fread(ptr, 1, (size_t)count, ((FileDisk*)f)->file); }

static void FileDisk_blockWrite(File* const f, uint8_t* const ptr, const uint64_t count) { fwrite(ptr, 1, (size_t)count, ((FileDisk*)f)->file); }

static void FileDisk_setpos(File* const f, const uint64_t newPos) { fseeko(((FileDisk*)f)->file, newPos, SEEK_SET); }

static void FileDisk_setEnd(File* const f) { fseeko(((FileDisk*)f)->file, 0, SEEK_END); }

static uint64_t FileDisk_curPos(File* const f) { return ftello(((FileDisk*)f)->file); }

static bool FileDisk_eof(File* const f) { return feof(((FileDisk*)f)->file) != 0; }

static const FileVtbl FileDisk_vtbl = {
  FileDisk_open, FileDisk_create, FileDisk_close, FileDisk_getChar, FileDisk_putChar,
  FileDisk_blockRead, FileDisk_blockWrite, FileDisk_setpos, FileDisk_setEnd, FileDisk_curPos, FileDisk_eof
};

void FileDisk_init(FileDisk* const f) {
  f->base.vt = &FileDisk_vtbl;
  f->file = NULL;
}
