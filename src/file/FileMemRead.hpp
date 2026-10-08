#pragma once

#include "File.hpp"
#include <cstring>

/**
 * Read-only File over a memory buffer (which it does not own).
 * Used by the extract-only stub to decompress a payload straight from the
 * package image, without writing the compressed payload to disk first.
 * Write operations are ignored.
 */
class FileMemRead : public File {
  const uint8_t* data;
  uint64_t size;
  uint64_t pos = 0;
public:
  FileMemRead(const uint8_t* buffer, uint64_t length) : data(buffer), size(length) {}
  bool open(const char*, bool) override { return true; }
  void create(const char*) override {}
  void close() override {}
  int getchar() override { return pos < size ? data[pos++] : EOF; }
  void putChar(uint8_t) override {}
  uint64_t blockRead(uint8_t* ptr, uint64_t count) override {
    const uint64_t n = count < size - pos ? count : size - pos;
    memcpy(ptr, data + pos, (size_t)n);
    pos += n;
    return n;
  }
  void blockWrite(uint8_t*, uint64_t) override {}
  void setpos(uint64_t newPos) override { pos = newPos < size ? newPos : size; }
  void setEnd() override { pos = size; }
  uint64_t curPos() override { return pos; }
  bool eof() override { return pos >= size; }
};
