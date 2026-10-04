#include "FileTmp.hpp"

FileTmp::FileTmp() : contentInRam(0), filePos(0), fileSize(0) {}

FileTmp::~FileTmp() { close(); }

bool FileTmp::open(const char* /*filename*/, bool /*mustSucceed*/) {
  assert(false);
  return false;
}

void FileTmp::create(const char* /*filename*/) { assert(false); }

void FileTmp::close() {
  contentInRam.resize(0);
  filePos = 0;
  fileSize = 0;
}

int FileTmp::getchar() {
  if (filePos >= fileSize) {
    return EOF;
  }
  return contentInRam[filePos++];
}

void FileTmp::putChar(uint8_t c) {
  if (filePos == fileSize) {
    contentInRam.pushBack(c);
    fileSize++;
  }
  else {
    contentInRam[filePos] = c;
  }
  filePos++;
}

uint64_t FileTmp::blockRead(uint8_t* ptr, uint64_t count) {
  const uint64_t available = fileSize - filePos;
  if (available < count) {
    count = available;
  }
  if (count > 0) {
    memcpy(ptr, &contentInRam[filePos], count);
    filePos += count;
  }
  return count;
}

void FileTmp::blockWrite(uint8_t* ptr, uint64_t count) {
  contentInRam.resize(filePos + count);
  if (count > 0) {
    memcpy(&contentInRam[filePos], ptr, count);
  }
  filePos += count;
  if (filePos > fileSize) {
    fileSize = filePos;
  }
}

void FileTmp::setpos(uint64_t newPos) {
  assert(newPos <= fileSize);
  filePos = newPos;
}

void FileTmp::setEnd() {
  filePos = fileSize;
}

uint64_t FileTmp::curPos() {
  return filePos;
}

bool FileTmp::eof() {
  return filePos >= fileSize;
}
