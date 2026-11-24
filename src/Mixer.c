#include "Mixer.h"
#include "Array.h"
#include "Squash.h"
#include "Stretch.h"

static int scaleDotProduct(const int dp, const int scaleFactor) {
  return (dp * scaleFactor) >> 16;
}

static int clipDotProduct(int dp) {
  if (dp < -2047) {
    dp = -2047;
  }
  else if (dp > 2047) {
    dp = 2047;
  }
  return dp;
}

static void addDotProductToNextMixer(Mixer* const mp, const int dp) {
  Mixer_add(mp, dp);
}

static int processDotProduct(Mixer* const mp, int dp, const int scaleFactor) {
  dp = scaleDotProduct(dp, scaleFactor);
  dp = clipDotProduct(dp);
  addDotProductToNextMixer(mp, dp);
  return squash(dp);
}


static int updateLearningRate(ErrorInfo* const info, int rate, const int err, const int lowerLimitOfLearningRate) {
  (void)info; (void)err;
  //linear learning rate decay
  if (rate > lowerLimitOfLearningRate) {
    rate--;
  }
  return rate;
}

/**
  * Adjusts weights to minimize the coding cost of the last prediction.
  * Trains the network where the expected output is the last bit (shared y).
  */
static void Mixer_update(IPredictor* const predictor) {
  Mixer* const mixer = (Mixer*)predictor;
  const Shared* const shared = mixer->shared;
  INJECT_SHARED_y
  const int target = y << 12;
  for (size_t i = 0; i < mixer->numContexts; ++i) {
    const int err = target - mixer->pr[i];
    int lim = mixer->mp == NULL ? 6 : 17;
    if (err < -lim || err > lim) { // skip training when error is low
      mixer->rates[i] = updateLearningRate(&mixer->info[i], mixer->rates[i], err, mixer->lowerLimitOfLearningRate);
      mixer->impl.train(mixer, &mixer->wx[mixer->cxt[i] * mixer->n], mixer->nx, (err * mixer->rates[i]) >> 16);
    }
  }
  Mixer_reset(mixer);
}

Mixer* Mixer_new(const Shared* const sh, const int n, const int m, const int s, const int promoted, const MixerImpl* const impl) {
  Mixer* const mixer = ARRAY_NEW(Mixer, 1); // zeroed
  mixer->impl = *impl;
  const int simdWidth = impl->simdWidth;
  mixer->predictor.update = Mixer_update;
  mixer->shared = sh;
  mixer->n = (n + (simdWidth - 1)) & -(simdWidth);
  mixer->m = m;
  mixer->s = s;
  mixer->lowerLimitOfLearningRate = s == 1 ? Mixer_MIN_LEARNING_RATE_S1 : Mixer_MIN_LEARNING_RATE_SN;
  mixer->tx = (short*)Array_new(mixer->n, sizeof(short), 64);
  mixer->wx = (short*)Array_new(mixer->n * m, sizeof(short), 64);
  mixer->cxt = ARRAY_NEW(uint32_t, s);
  mixer->info = ARRAY_NEW(ErrorInfo, s);
  mixer->rates = ARRAY_NEW(int, s);
  mixer->pr = ARRAY_NEW(int, s);
  assert((mixer->n & (simdWidth - 1)) == 0);
  assert(mixer->m > 0);
  assert(mixer->s > 0);
  for (size_t i = 0; i < (size_t)s; ++i) {
    mixer->pr[i] = 2048; //initial p=0.5
    mixer->rates[i] = Mixer_MAX_LEARNING_RATE;
  }
  const short initialWeight = s == 1 ? 8192 : 128;
  for (size_t i = 0; i < mixer->n * m; ++i) {
    mixer->wx[i] = initialWeight;
  }
  Mixer_reset(mixer);
  if (s > 1) {
    mixer->mp = Mixer_new(sh, s + promoted, 1, 1, 0, impl);
  }
  return mixer;
}

void Mixer_setScaleFactor(Mixer* const mixer, const int sf0, const int sf1) {
  mixer->scaleFactor = sf0;
  if (mixer->mp != NULL) {
    Mixer_setScaleFactor(mixer->mp, sf1, 0);
  }
}

void Mixer_setLowerLimitOfLearningRate(Mixer* const mixer, const int lr0, const int lr1) {
  mixer->lowerLimitOfLearningRate = lr0 * 65536;
  if (mixer->mp != NULL) {
    Mixer_setLowerLimitOfLearningRate(mixer->mp, lr1, 0);
  }
}

void Mixer_promote(Mixer* const mixer, const int x) {
  if (mixer->mp != NULL) {
    Mixer_add(mixer->mp, x);
  }
}

int Mixer_p(Mixer* const mixer) {
  UpdateBroadcaster_subscribe(Shared_getUpdateBroadcaster(mixer->shared), &mixer->predictor);
  assert(mixer->scaleFactor > 0);
  //pad input to a multiple of simdWidth
  while (mixer->nx & (mixer->impl.simdWidth - 1)) {
    mixer->tx[mixer->nx++] = 0;
  }
  const size_t n = mixer->n;
  const size_t nx = mixer->nx;
  if (mixer->mp != NULL) { // first mixer layer: feed results to second layer
    for (size_t i = 0; i < mixer->numContexts; i++) {
      const int dp = mixer->impl.dotProduct(mixer, &mixer->wx[mixer->cxt[i] * n], nx);
      mixer->pr[i] = processDotProduct(mixer->mp, dp, mixer->scaleFactor);
    }
    Mixer_set(mixer->mp, 0, 1);
    return Mixer_p(mixer->mp);
  }
  else { // second (last) mixer layer: return prediction directly
    const int dp = scaleDotProduct(mixer->impl.dotProduct(mixer, &mixer->wx[mixer->cxt[0] * n], nx), mixer->scaleFactor);
    return mixer->pr[0] = squash(dp);
  }
}

void Mixer_addProbability(Mixer* const mixer, const int p1, const int scale) {
  Mixer_add(mixer, (stretch(p1) * scale) >> 8);
  Mixer_add(mixer, ((p1 - 2048) * scale) >> 9);
}

void Mixer_set(Mixer* const mixer, const uint32_t cx, const uint32_t range) {
  assert(mixer->numContexts < mixer->s);
  assert(cx < range);
  assert(mixer->base + range <= mixer->m);
  mixer->cxt[mixer->numContexts++] = mixer->base + cx;
  mixer->base += range;
}

void Mixer_reset(Mixer* const mixer) {
  mixer->nx = 0;
  mixer->base = 0;
  mixer->numContexts = 0;
}
