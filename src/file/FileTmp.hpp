#pragma once

#include "File.hpp"

/**
 * This class is responsible for temporary files in RAM.
 * All file operations use RAM exclusively.
 */
class FileTmp : public File {
private:
  Array<uint8_t> contentInRam; /**< content of file */
  uint64_t filePos;
  uint64_t fileSize;

public:
  FileTmp();
  ~FileTmp() override;

  /**
    *  This method is forbidden for temporary files.
    */
  bool open(const char * /*filename*/, bool /*mustSucceed*/) override;

  /**
    *  This method is forbidden for temporary files.
    */
  void create(const char * /*filename*/) override;
  void close() override;
  int getchar() override;
  void putChar(uint8_t c) override;
  uint64_t  blockRead(uint8_t *ptr, uint64_t count) override;
  void blockWrite(uint8_t *ptr, uint64_t count) override;
  void setpos(uint64_t newPos) override;
  void setEnd() override;
  uint64_t curPos() override;
  bool  eof() override;
};
