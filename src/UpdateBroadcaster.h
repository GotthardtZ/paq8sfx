#pragma once

#include "IPredictor.h"
#include "SystemDefines.h"

/**
 * The purpose of this class is to inform probability predictors when
 * the next bit is known by calling the update() method of each predictor.
 */
typedef struct UpdateBroadcaster {
  int n; /**< number of subscribed predictors, (number of items in "subscribers" array) */
  IPredictor* subscribers[1024];
} UpdateBroadcaster;

void UpdateBroadcaster_subscribe(UpdateBroadcaster* ub, IPredictor* subscriber);
void UpdateBroadcaster_broadcastUpdate(UpdateBroadcaster* ub);
