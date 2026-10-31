#pragma once

#include "Array.h"
#include "DivisionTable.h"
#include "Shared.h"

/**
 * This is the base class for StateMap and APM.
 * Purpose: common members are here
 */
typedef struct AdaptiveMap {
  const Shared* shared;
  uint32_t* t; /**< cxt -> prediction in high 22 bits, count in low 10 bits */
  int* dt; /**< Pointer to division table */
} AdaptiveMap;

void AdaptiveMap_init(AdaptiveMap* am, const Shared* sh, int n);
void AdaptiveMap_destroy(AdaptiveMap* am);
void AdaptiveMap_update(const AdaptiveMap* am, uint32_t* p, int limit);
