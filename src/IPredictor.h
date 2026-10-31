#pragma once

/**
 * Common interface for all probability predictors (probability map and mixer classes).
 * It declares the update() event handler.
 * A probability predictor predicts the probability that the next bit is 1.

 * After each bit becomes known, all probability predictors participated
 * in the prediction must update their internal state to improve future
 * predictions (adapting). In order to achieve this, a probability
 * predictor must
 *   1) have an IPredictor as its first member,
 *   2) set its update() function pointer to its update function,
 *   3) subscribe for the broadcast in p() or mix(),
 *   4) receive the update event from UpdateBroadcaster and update its internal state in update().
 * The update function gets the IPredictor, which is the address of the predictor itself.
 */
typedef struct IPredictor IPredictor;
struct IPredictor {
  void (*update)(IPredictor* self);
};
