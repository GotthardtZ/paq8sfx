#pragma once

#include "PredictorMain.h"
#include "ArithmeticEncoder.h"
#include "Shared.h"

typedef enum {
    COMPRESS, DECOMPRESS
} Mode;

/**
 * An Encoder encodes or decodes bytes using a Predictor and arithmetic encoding.
 * If shared->level is 0, then data is stored in 'archive' without arithmetic coding.
 */
typedef struct Encoder {
  ArithmeticEncoder ari;
  Mode mode; /**< Compress or decompress? */
  File* archive; /**< Compressed data file */
#ifdef FULL
  File* alt; /**< decompressByte() source in COMPRESS mode */
#endif
  float p1, p2; /**< percentages for progress indicator: 0.0 .. 1.0 */
  PredictorMain* predictorMain;
} Encoder;

/**
  * Encoder_init(COMPRESS, f) creates encoder for compression to archive @ref f, which
  * must be open past any header for writing in binary mode.
  * Encoder_init(DECOMPRESS, f) creates encoder for decompression from archive @ref f,
  * which must be open past any header for reading in binary mode.
  * @param m the mode to operate in
  * @param f the file to read from or write to
  */
void Encoder_init(Encoder* en, PredictorMain* predictorMain, Mode m, File* f);
Mode Encoder_getMode(const Encoder* en);

/**
  * Returns current length of archive
  * @return length of archive so far
  */
uint64_t Encoder_size(const Encoder* en);

#ifdef FULL
/**
  * Should be called exactly once after compression is done and
  * before closing @ref f. It does nothing in DECOMPRESS mode.
  */
void Encoder_flush(Encoder* en);

/**
  * Sets alternate source to @ref f for decompressByte() in COMPRESS mode (for testing transforms).
  * @param f
  */
void Encoder_setFile(Encoder* en, File* f);

/**
  * compressByte(c) in COMPRESS mode compresses one byte.
  * @param c the byte to be compressed
  */
void Encoder_compressByte(Encoder* en, PredictorMain* predictor, uint8_t c);
#endif // FULL

/**
  * decompressByte() in DECOMPRESS mode decompresses and returns one byte.
  * @return the decompressed byte
  */
uint8_t Encoder_decompressByte(Encoder* en, PredictorMain* predictor);

void Encoder_initContextForBlockModel(Encoder* en, BlockType blockType, int blockInfo);

void Encoder_setStatusRange(Encoder* en, float perc1, float perc2);
#ifdef FULL
void Encoder_printStatusRange(const Encoder* en, uint64_t n, uint64_t size);
#endif
void Encoder_printStatus(const Encoder* en);
