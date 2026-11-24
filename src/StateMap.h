#pragma once

#include "AdaptiveMap.h"
#include "IPredictor.h"
#include "StateTable.h"
#include "UpdateBroadcaster.h"

typedef enum StateMapType {
  STATEMAP_GENERIC, STATEMAP_BITHISTORY, STATEMAP_RUN
} StateMapType;

/**
 * A @ref StateMap maps a context to a probability.
 */
typedef struct StateMap {
  IPredictor predictor;
  AdaptiveMap am;
  uint32_t numContextSets; /**< Number of context sets */
  uint32_t numContextsPerSet; /**< Number of contexts in each context set */
  uint32_t currentContextSetIndex; /**< Number of context indexes present in cxt array (0..numContextSets-1) */
  uint32_t* cxt; /**< context index of last prediction per context set */
  int limit; //1..1023
} StateMap;

/**
  * Creates a @ref StateMap with @ref n contexts using 4*n bytes memory.
  * @param s
  * @param n number of contexts
  * @param lim
  * @param mapType
  */
void StateMap_init(StateMap* sm, const Shared* sh, int s, int n, int lim, StateMapType mapType);
/**
  * Call @ref StateMap_p1() when there is only 1 context set.
  * No need to call @ref StateMap_subscribe().
  * @param cx
  * @return
  */
int StateMap_p1(StateMap* sm, uint32_t cx);

/**
  * Call @ref StateMap_p2() for each context when there are more context sets, finally call @ref StateMap_subscribe() once.
  * sm.p(y, cx, limit) converts state @ref cx (0..n-1) to a probability (0..4095)
  * that the next y=1, updating the previous prediction with y (0..1).
  * limit (1..1023, default 1023) is the maximum count for computing a
  * prediction.  Larger values are better for stationary sources.
  * @param s
  * @param cx
  * @return
  */
int StateMap_p2(StateMap* sm, uint32_t s, uint32_t cx);
void StateMap_subscribe(StateMap* sm);

/**
  * Call @ref StateMap_skip() instead of @ref StateMap_p2() when the context is unknown or uninteresting.
  * @remark no need to call @ref StateMap_skip() when there is only 1 context or all contexts will be skipped.
  * @param s
  */
void StateMap_skip(StateMap* sm, uint32_t s);
void StateMap_print(const StateMap* sm);
