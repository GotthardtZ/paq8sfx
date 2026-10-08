#pragma once

#include "../file/File.hpp"
#include "../Encoder.hpp"
#include <cstdint>

/**
 * What to do with the decoded data:
 * write it to the output file, compare it with the output file, or drop it.
 */
enum class FMode {
  FDECOMPRESS, FCOMPARE, FDISCARD
};

/**
 * A filter transforms a block of a certain type into a better compressible
 * form (encode) and back (decode).
 */
class Filter {
protected:
  Encoder *encoder = nullptr;
public:
  virtual void encode(File *in, File *out, uint64_t size, int info, int &headerSize) = 0;
  virtual uint64_t decode(File *in, File *out, FMode fMode, uint64_t size, uint64_t &diffFound) = 0;

  void setEncoder(Encoder &en) {
    encoder = &en;
  }

  virtual ~Filter() = default;
};
