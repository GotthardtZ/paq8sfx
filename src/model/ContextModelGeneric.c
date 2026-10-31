#include "../MixerFactory.hpp"
#include "ChartModel.hpp"
#include "ExeModel.hpp"
#include "MatchModel.hpp"
#include "NormalModel.hpp"
#include "SimilarityModelPair.hpp"
#include "SparseBitModel.hpp"
#include "SparseModel.hpp"
#include "IContextModel.hpp"

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
class ContextModelGeneric: public IContextModel {

private:
  Shared* const shared;
  Mixer* m;

  NormalModel normalModel;
  MatchModel matchModel;
  SparseBitModel sparseBitModel;
  SparseModel sparseModel;
  ChartModel chartModel;
  SimilarityModelPair similarityModelPair;
  ExeModel exeModel;

public:
  ContextModelGeneric(Shared* const sh, const MixerFactory* const mf) :
    shared(sh),
    m(mf->createMixer(
      1 +  //bias
      MatchModel::MIXERINPUTS + NormalModel::MIXERINPUTS +
      SparseModel::MIXERINPUTS + SparseBitModel::MIXERINPUTS + ChartModel::MIXERINPUTS +
      2 * SimilarityModel::MIXERINPUTS + ExeModel::MIXERINPUTS
      ,
      MatchModel::MIXERCONTEXTS + NormalModel::MIXERCONTEXTS_PRE + NormalModel::MIXERCONTEXTS_POST +
      SparseModel::MIXERCONTEXTS + SparseBitModel::MIXERCONTEXTS + ChartModel::MIXERCONTEXTS +
      2 * SimilarityModel::MIXERCONTEXTS + ExeModel::MIXERCONTEXTS
      ,
      MatchModel::MIXERCONTEXTSETS + NormalModel::MIXERCONTEXTSETS_PRE + NormalModel::MIXERCONTEXTSETS_POST +
      SparseModel::MIXERCONTEXTSETS + SparseBitModel::MIXERCONTEXTSETS + ChartModel::MIXERCONTEXTSETS +
      2 * SimilarityModel::MIXERCONTEXTSETS + ExeModel::MIXERCONTEXTSETS
      ,
      0
    )),
    normalModel{ sh, sh->mem * 32 },
    matchModel{ sh, sh->mem / 4 /*hashtablesize*/, sh->mem /*mapmemorysize*/ }, /**< Not the actual memory use - see in the model */
    sparseBitModel{ sh, sh->mem / 4 },
    sparseModel{ sh, sh->mem * 4 },
    chartModel{ sh, sh->mem * 4 },
    similarityModelPair{ sh, sh->mem },
    exeModel{ sh, sh->mem * 4 }
  {
    m->setScaleFactor(1150, 100);
  }

  int p() {
    normalModel.mix(*m);
    normalModel.mixPost(*m);
    matchModel.mix(*m);
    sparseBitModel.mix(*m);
    sparseModel.mix(*m);
    chartModel.mix(*m);
    similarityModelPair.mix(*m);
    //exemodel must be the last
    exeModel.mix(*m);
    return m->p();
  }

  ~ContextModelGeneric() {
    delete m;
  }

};
