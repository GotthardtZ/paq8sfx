#pragma once

#include "../BlockType.hpp"
#include "../Mixer.hpp"
#include "../MixerFactory.hpp"
#include "IContextModel.hpp"

class ContextModelGeneric;

/**
 * This combines all the context models with a Mixer.
 * Owns the context model it selects (created on first use).
 */
class ContextModel {
  Shared * const shared;
  const MixerFactory* const mixerFactory;

  ContextModelGeneric* contextModelGeneric = nullptr;
  IContextModel* selectedContextModel = nullptr;

public:
  ContextModel(Shared* const sh, const MixerFactory* const mixerFactory);
  ~ContextModel();
  int p();
};
