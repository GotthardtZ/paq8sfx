#pragma once

#include "AdaptiveMap.h"
#include "IPredictor.h"

/**
 * An APM maps a probability and a context to a new probability.
 */
typedef struct APM {
  IPredictor predictor;
  AdaptiveMap am;
  int N; /**< Number of contexts */
  int steps;
  int cxt; /**< context index of last prediction */
  int limit; //1..1023
} APM;

/**
  * Creates with @ref n contexts using 4*s*n bytes memory.
  * @param n the number of contexts
  * @param s the number of steps
  */
void APM_init(APM* apm, const Shared* sh, int n, int s, int limit);
void APM_destroy(APM* apm);

/**
  * Returns a new probability (0..4095) like with @ref StateMap.
  * cx=(0..n-1) is the context.
  * pr=(0..4095) is considered part of the context.
  * The output is computed by interpolating @ref pr into @ref steps ranges nonlinearly
  * with smaller ranges near the ends. The initial output is pr.
  * lim=(0..1023): set a lower limit (like 255) for faster adaptation.
  * @param pr the initial probability. Considered part of the context.
  * @param cx the context
  * @return the adjusted probability
  */
int APM_p(APM* apm, int pr, int cx);
