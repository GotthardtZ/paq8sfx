#pragma once

/**
 * What to do with the decoded data:
 * write it to the output file, compare it with the output file, or drop it.
 */
typedef enum FMode {
  FDECOMPRESS, FCOMPARE, FDISCARD
} FMode;

// A filter transforms a block of a certain type into a better compressible
// form (encode) and back (decode). The only filter is the EXE filter
// (exe.h).
