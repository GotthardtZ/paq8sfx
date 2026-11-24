#pragma once

#include "../MixerFactory.h"
#include "ChartModel.h"
#include "ExeModel.h"
#include "MatchModel.h"
#include "NormalModel.h"
#include "SimilarityModelPair.h"
#include "SparseBitModel.h"
#include "SparseModel.h"

/*
 relationship between compression level, shared->mem and NormalModel memory use as an example

 level   shared->mem    NormalModel memory use (shared->mem*32)
 -----   -----------    ----------------------
   1      0.125 MB              4 MB
   2      0.25  MB              8 MB
   3      0.5   MB             16 MB
   4      1.0   MB             32 MB
   5      2.0   MB             64 MB
   6      4.0   MB            128 MB
   7      8.0   MB            256 MB
   8     16.0   MB            512 MB
   9     32.0   MB           1024 MB
  10     64.0   MB           2048 MB
  11    128.0   MB           4096 MB
  12    256.0   MB           8192 MB
*/

/**
 * The context model: all the models combined with a Mixer.
 *
 * It owns its models (created with it, destroyed with it), so the whole model
 * state belongs to one PredictorMain - a process can decompress any number of
 * archives one after the other (unlike paq8px).
 */
typedef struct ContextModelGeneric {
  Shared* shared;
  Mixer* m;
  ExeModel* exeModel;

  NormalModel normalModel;
  MatchModel matchModel;
  SparseBitModel sparseBitModel;
  SparseModel sparseModel;
  ChartModel chartModel;
  SimilarityModelPair similarityModelPair;
} ContextModelGeneric;

ContextModelGeneric* ContextModelGeneric_new(Shared* sh);
int ContextModelGeneric_p(ContextModelGeneric* cmg);
