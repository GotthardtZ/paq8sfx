#pragma once

#include "IPredictor.h"
#include "Shared.h"

/**
 * APM1 maps a probability and a context into a new probability that bit y will next be 1.
 * After each guess it updates its state to improve future guesses.
 * Probabilities are scaled 12 bits (0-4095).
 */
typedef struct APM1 {
  IPredictor predictor;
  const Shared* shared;
  int index; /**< last p, context */
  int n; /**< number of contexts */
  uint16_t* t; /**< [n][33]:  p, context -> p */
  int rate;
} APM1;

/**
  * Creates an instance with @ref n contexts and learning rate @ref r.
  * @ref r determines the learning rate. Smaller = faster, must be in the range (0, 32).
  * Uses 66*n bytes memory.
  * @param n the number of contexts
  * @param r the learning rate
  */
void APM1_init(APM1* apm, const Shared* sh, int n, int r);
/**
  * Returns adjusted probability in context @ref cx (0 to n-1).
  * @param pr initial (pre-adjusted) probability
  * @param cxt the context
  * @return adjusted probability
  */
int APM1_p(APM1* apm, int pr, int cxt);
