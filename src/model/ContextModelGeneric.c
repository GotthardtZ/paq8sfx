#include "../Array.h"
#include "ContextModelGeneric.h"

ContextModelGeneric* ContextModelGeneric_new(Shared* const sh) {
  ContextModelGeneric* const cmg = ARRAY_NEW(ContextModelGeneric, 1); // zeroed
  cmg->shared = sh;
  cmg->m = MixerFactory_createMixer(sh,
    1 +  //bias
    MatchModel_MIXERINPUTS + NormalModel_MIXERINPUTS +
    SparseModel_MIXERINPUTS + SparseBitModel_MIXERINPUTS + ChartModel_MIXERINPUTS +
    2 * SimilarityModel_MIXERINPUTS + ExeModel_MIXERINPUTS
    ,
    MatchModel_MIXERCONTEXTS + NormalModel_MIXERCONTEXTS_PRE + NormalModel_MIXERCONTEXTS_POST +
    SparseModel_MIXERCONTEXTS + SparseBitModel_MIXERCONTEXTS + ChartModel_MIXERCONTEXTS +
    2 * SimilarityModel_MIXERCONTEXTS + ExeModel_MIXERCONTEXTS
    ,
    MatchModel_MIXERCONTEXTSETS + NormalModel_MIXERCONTEXTSETS_PRE + NormalModel_MIXERCONTEXTSETS_POST +
    SparseModel_MIXERCONTEXTSETS + SparseBitModel_MIXERCONTEXTSETS + ChartModel_MIXERCONTEXTSETS +
    2 * SimilarityModel_MIXERCONTEXTSETS + ExeModel_MIXERCONTEXTSETS
    ,
    0
  );
  NormalModel_init(&cmg->normalModel, sh, sh->mem * 32);
  MatchModel_init(&cmg->matchModel, sh, sh->mem / 4 /*hashtablesize*/, sh->mem /*mapmemorysize*/); /**< Not the actual memory use - see in the model */
  SparseBitModel_init(&cmg->sparseBitModel, sh, sh->mem / 4);
  SparseModel_init(&cmg->sparseModel, sh, sh->mem * 4);
  ChartModel_init(&cmg->chartModel, sh, sh->mem * 4);
  SimilarityModelPair_init(&cmg->similarityModelPair, sh, sh->mem);
  cmg->exeModel = ExeModel_new(sh, sh->mem * 4);
  Mixer_setScaleFactor(cmg->m, 1150, 100);
  return cmg;
}

int ContextModelGeneric_p(ContextModelGeneric* const cmg) {
  Mixer* const m = cmg->m;
  NormalModel_mix(&cmg->normalModel, m);
  NormalModel_mixPost(&cmg->normalModel, m);
  MatchModel_mix(&cmg->matchModel, m);
  SparseBitModel_mix(&cmg->sparseBitModel, m);
  SparseModel_mix(&cmg->sparseModel, m);
  ChartModel_mix(&cmg->chartModel, m);
  SimilarityModelPair_mix(&cmg->similarityModelPair, m);
  //exemodel must be the last
  ExeModel_mix(cmg->exeModel, m);
  return Mixer_p(m);
}
