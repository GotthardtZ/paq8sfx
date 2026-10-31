#pragma once

#include "IPredictor.h"
#include "Shared.h"

/**
 * APMPost maps a 12-bit probability (0-4095) to an n-bit probability (as defined by the Arithmetic Encoder)
 * After each guess it updates its state to improve future guesses.
 */
typedef struct APMPost {
  IPredictor predictor;
  const Shared* shared;
  uint32_t index; /**< last p, context */
  uint32_t n; /**< number of contexts */
  uint64_t* t;
} APMPost;

/**
  * Creates an instance with @ref n contexts.
  * @param n the number of contexts
  */
void APMPost_init(APMPost* apm, const Shared* sh, uint32_t n);
void APMPost_destroy(APMPost* apm);

/**
  * Returns adjusted probability in context @ref cx (0 to n-1).
  * @param pr initial (pre-adjusted) probability
  * @param cxt the context
  * @return adjusted probability
  */
uint32_t APMPost_p(APMPost* apm, uint32_t pr, uint32_t cxt);

void APMPost_print(const APMPost* apm);
