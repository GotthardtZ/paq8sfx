#pragma once

#include "model/ChartModel.hpp"
#include "model/SimilarityModelPair.hpp"
#include "model/ExeModel.hpp"
#include "model/MatchModel.hpp"
#include "model/NormalModel.hpp"
#include "model/SparseBitModel.hpp"
#include "model/SparseModel.hpp"
#include "model/IContextModel.hpp"
#include "MixerFactory.hpp"

/**
 * This is a factory class for lazy object creation for models.
 * Objects created within this class are instantiated on first use and guaranteed to be destroyed.
 */
class Models {
private:
  Shared* const shared;
  const MixerFactory* const mixerFactory;
public:
  explicit Models(Shared* const sh, MixerFactory* mf);
  auto normalModel() -> NormalModel&;
  auto similarityModelPair() -> SimilarityModelPair&;
  auto chartModel() -> ChartModel&;
  auto sparseBitModel() -> SparseBitModel&;
  auto sparseModel() -> SparseModel&;
  auto matchModel() -> MatchModel&;
    auto exeModel() -> ExeModel&;
};
