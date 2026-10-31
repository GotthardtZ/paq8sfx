#pragma once

#include "../ContextMap2.h"

/**
 * Model for order 0-14 contexts
 * Contexts are hashes of previous 0..14 bytes.
 * Order 0..6, 8 and 14 are used for prediction.
 * Note: order 7+ contexts are modeled by matchModel as well.
 */
enum {
  NormalModel_nCM = 9,
  NormalModel_MIXERINPUTS = NormalModel_nCM * (ContextMap2_MIXERINPUTS), // 63
  NormalModel_MIXERCONTEXTS_PRE = 64,
  NormalModel_MIXERCONTEXTS_POST = 1024 + 256 + 512 + 256 + 256 + 1536, //3840
  NormalModel_MIXERCONTEXTSETS_PRE = 1,
  NormalModel_MIXERCONTEXTSETS_POST = 6
};

typedef struct NormalModel {
  Shared* shared;
  ContextMap2 cm;
} NormalModel;

void NormalModel_init(NormalModel* nm, Shared* sh, uint64_t cmSize);
void NormalModel_destroy(NormalModel* nm);
void NormalModel_reset(NormalModel* nm);

/**
  * update order 1..14 context hashes.
  * Note: order 0 context does not need an update so its hash never changes.
  */
void NormalModel_updateHashes(NormalModel* nm);
void NormalModel_mix(NormalModel* nm, Mixer* m);

/**
  * setting more mixer contexts in the generic case (i.e. not using the special models such as image, audio, jpeg models)
  * @param m
  */
void NormalModel_mixPost(NormalModel* nm, Mixer* m);
