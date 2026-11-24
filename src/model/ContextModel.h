#pragma once

#include "../Shared.h"
#include "ContextModelGeneric.h"

/**
 * This combines all the context models with a Mixer.
 * Owns the context model it selects (created on first use).
 */
typedef struct ContextModel {
  Shared* shared;
  ContextModelGeneric* contextModelGeneric;
  ContextModelGeneric* selectedContextModel;
} ContextModel;

void ContextModel_init(ContextModel* cm, Shared* sh);
int ContextModel_p(ContextModel* cm);
