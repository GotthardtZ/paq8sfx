#include "UpdateBroadcaster.h"

void UpdateBroadcaster_subscribe(UpdateBroadcaster* const ub, IPredictor* const subscriber) {
  ub->subscribers[ub->n] = subscriber;
  ub->n++;
  assert(ub->n < 1024);
}

void UpdateBroadcaster_broadcastUpdate(UpdateBroadcaster* const ub) {
  for( int i = 0; i < ub->n; i++ ) {
    IPredictor* subscriber = ub->subscribers[i];
    subscriber->update(subscriber);
  }
  ub->n = 0;
}
