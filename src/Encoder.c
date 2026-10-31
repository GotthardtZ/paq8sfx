#include "Encoder.h"

void Encoder_init(Encoder* const en, PredictorMain* const predictorMain, const Mode m, File* const f) {
  ArithmeticEncoder_init(&en->ari, f);
  en->mode = m;
  en->archive = f;
#ifdef FULL
  en->alt = NULL;
#endif
  en->p1 = 0;
  en->p2 = 0;
  en->predictorMain = predictorMain;
  if( en->mode == DECOMPRESS ) {
    uint64_t start = Encoder_size(en);
    File_setEnd(en->archive);
    uint64_t end = Encoder_size(en);
    if( end >= (UINT64_C(1) << 31)) {
      quit("Large archives not yet supported.");
    }
    Encoder_setStatusRange(en, 0.0, (float)end);
    File_setpos(en->archive, start);
  }
  if( en->mode == DECOMPRESS ) { // x = first 4 bytes of archive
    ArithmeticEncoder_prefetch(&en->ari);
  }
}

Mode Encoder_getMode(const Encoder* const en) { return en->mode; }

uint64_t Encoder_size(const Encoder* const en) { return File_curPos(en->archive); }

static void updateModels(PredictorMain* const predictor, const uint32_t p, const int y) {
  bool isMissed = ((p >> (ArithmeticEncoder_PRECISION - 1)) != (uint32_t)y);
  Shared_update(predictor->shared, y, p, isMissed);
}

#ifdef FULL
void Encoder_flush(Encoder* const en) {
  if(en->mode == COMPRESS) {
    ArithmeticEncoder_flush(&en->ari);
  }
}

void Encoder_setFile(Encoder* const en, File* const f) { en->alt = f; }

void Encoder_compressByte(Encoder* const en, PredictorMain* const predictor, const uint8_t c) {
  assert(en->mode == COMPRESS);
  for( int i = 7; i >= 0; --i ) {
    uint32_t p = PredictorMain_p(predictor);
    int y = (c >> i) & 1;
    ArithmeticEncoder_encodeBit(&en->ari, p, y);
    updateModels(predictor, p, y);
  }
  assert(predictor->shared->State.c1 == c);
}
#endif // FULL

uint8_t Encoder_decompressByte(Encoder* const en, PredictorMain* const predictor) {
#ifdef FULL
  if( en->mode == COMPRESS ) {
    assert(en->alt);
    return (uint8_t)File_getChar(en->alt);
  }
#endif
  for( int i = 0; i < 8; ++i ) {
    int p = PredictorMain_p(predictor);
    int y = ArithmeticEncoder_decodeBit(&en->ari, p);
    updateModels(predictor, p, y);
  }
  return predictor->shared->State.c1;
}

void Encoder_initContextForBlockModel(Encoder* const en, const BlockType blockType, const int blockInfo) {
  Shared* shared = en->predictorMain->shared;
  shared->State.blockType = blockType;
  shared->State.blockInfo = blockInfo;
  shared->State.blockPos = UINT32_MAX;
}

void Encoder_setStatusRange(Encoder* const en, const float perc1, const float perc2) {
  en->p1 = perc1;
  en->p2 = perc2;
}

#ifdef FULL
void Encoder_printStatusRange(const Encoder* const en, const uint64_t n, const uint64_t size) {
  fprintf(stderr, "%6.2f%%\b\b\b\b\b\b\b", (en->p1 + (en->p2 - en->p1) * n / (size + 1)) * 100);
  fflush(stderr);
}
#endif

void Encoder_printStatus(const Encoder* const en) {
  fprintf(stderr, "%6.2f%%\b\b\b\b\b\b\b", (float)(Encoder_size(en)) / (en->p2 + 1) * 100);
  fflush(stderr);
}
