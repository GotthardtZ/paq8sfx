#pragma once

#include "SSE.h"
#include "Shared.h"
#include "model/ContextModel.h"

/**
 * A Predictor which estimates the probability that the next bit of uncompressed data is 1 
 * in the main data stream with the help of model(s), mixer(s) and an SSE stage. 
 */
typedef struct PredictorMain {
  ContextModel contextModel;
  SSE sse;
  Shared* shared;
} PredictorMain;

void PredictorMain_init(PredictorMain* predictor, Shared* sh);
uint32_t PredictorMain_p(PredictorMain* predictor);
