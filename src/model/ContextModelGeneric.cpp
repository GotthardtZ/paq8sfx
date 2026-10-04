#include "../MixerFactory.hpp"
#include "../Models.hpp"

class ContextModelGeneric: public IContextModel {

private:
  Shared* const shared;
  Models* const models;
  Mixer* m;

public:
  ContextModelGeneric(Shared* const sh, Models* const models, const MixerFactory* const mf) : shared(sh), models(models) {
    m = mf->createMixer(
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
    );
    m->setScaleFactor(1150, 100);
  }

  int p() {

    NormalModel& normalModel = models->normalModel();
    normalModel.mix(*m);
    normalModel.mixPost(*m);

    MatchModel& matchModel = models->matchModel();
    matchModel.mix(*m);

    SparseBitModel& sparseBitModel = models->sparseBitModel();
    sparseBitModel.mix(*m);
    SparseModel& sparseModel = models->sparseModel();
    sparseModel.mix(*m);
    ChartModel& chartModel = models->chartModel();
    chartModel.mix(*m);

    SimilarityModelPair& similarityModelPair = models->similarityModelPair();
    similarityModelPair.mix(*m);

    //exemodel must be the last
    ExeModel& exeModel = models->exeModel();
    exeModel.mix(*m);

    return m->p();
  }

  ~ContextModelGeneric() {
    delete m;
  }

};
