#pragma once

#include "File.hpp"
#include "fileUtils.hpp"

/**
 * This class is responsible for files on disk.
 * It simply passes function calls to stdio.
 */
class FileDisk : public File {
protected:
  FILE *file;

public:
  FileDisk();
  ~FileDisk() override;
  bool open(const char *filename, bool mustSucceed) override;
  void create(const char *filename) override;
  void close() override;
  int getchar() override;
  void putChar(uint8_t c) override;
  uint64_t blockRead(uint8_t *ptr, uint64_t count) override;
  void blockWrite(uint8_t *ptr, uint64_t count) override;
  void setpos(uint64_t newPos) override;
  void setEnd() override;
  uint64_t curPos() override;
  bool  eof() override;
};
