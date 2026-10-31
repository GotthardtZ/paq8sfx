#include "PredictorMain.h"
#include "ArithmeticEncoder.h"

void PredictorMain_init(PredictorMain* const predictor, Shared* const sh) {
  predictor->shared = sh;
  SSE_init(&predictor->sse, sh);
  Shared_reset(predictor->shared);
  ContextModel_init(&predictor->contextModel, sh);
}

void PredictorMain_destroy(PredictorMain* const predictor) {
  ContextModel_destroy(&predictor->contextModel);
  SSE_destroy(&predictor->sse);
}

uint32_t PredictorMain_p(PredictorMain* const predictor) {
  uint32_t pr = ContextModel_p(&predictor->contextModel);
  pr = SSE_p(&predictor->sse, pr);
  return pr;
}
