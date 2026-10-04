#include "Mixer.hpp"

#include "Squash.hpp"

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
  mp->add(dp);
}

static int processDotProduct(Mixer* const mp, int dp, const int scaleFactor) {
  dp = scaleDotProduct(dp, scaleFactor);
  dp = clipDotProduct(dp);
  addDotProductToNextMixer(mp, dp);
  return squash(dp);
}


static int updateLearningRate(ErrorInfo& info, int rate, const int err, const int lowerLimitOfLearningRate) {
  //linear learning rate decay
  if (rate > lowerLimitOfLearningRate) {
    rate--;
  }
  return rate;
}

Mixer::Mixer(const Shared* const sh, const int n, const int m, const int s, const int simdWidth) :
  shared(sh),
  n((n + (simdWidth - 1)) & -(simdWidth)),
  m(m), s(s),
  lowerLimitOfLearningRate(s == 1 ? MIN_LEARNING_RATE_S1 : MIN_LEARNING_RATE_SN),
  scaleFactor(0),
  tx((n + (simdWidth - 1)) & -(simdWidth)),
  wx(((n + (simdWidth - 1)) & -(simdWidth))* m),
  cxt(s), info(s), rates(s), pr(s),
  mp(nullptr),
  simdWidth(simdWidth) {
  assert((this->n & (simdWidth - 1)) == 0);
  assert(this->m > 0);
  assert(this->s > 0);
  for (size_t i = 0; i < s; ++i) {
    pr[i] = 2048; //initial p=0.5
    rates[i] = MAX_LEARNING_RATE;
  }
  const short initialWeight = s == 1 ? 8192 : 128;
  for (size_t i = 0; i < this->n * m; ++i) {
    wx[i] = initialWeight;
  }
  reset();
}

Mixer::~Mixer() {
  delete mp;
}

void Mixer::setScaleFactor(const int sf0, const int sf1) {
  scaleFactor = sf0;
  if (mp != nullptr) {
    mp->setScaleFactor(sf1, 0);
  }
}

void Mixer::setLowerLimitOfLearningRate(const int lr0, const int lr1) {
  lowerLimitOfLearningRate = lr0 * 65536;
  if (mp != nullptr) {
    mp->setLowerLimitOfLearningRate(lr1, 0);
  }
}

void Mixer::promote(const int x) {
  if (mp != nullptr) {
    mp->add(x);
  }
}

void Mixer::update() {
  INJECT_SHARED_y
    const int target = y << 12;
  for (size_t i = 0; i < numContexts; ++i) {
    const int err = target - pr[i];
    int lim = mp == nullptr ? 6 : 17;
    if (err < -lim || err > lim) { // skip training when error is low
      rates[i] = updateLearningRate(info[i], rates[i], err, lowerLimitOfLearningRate);
      train(&wx[cxt[i] * n], nx, (err * rates[i]) >> 16);
    }
  }
  reset();
}

int Mixer::p() {
  shared->GetUpdateBroadcaster()->subscribe(this);
  assert(scaleFactor > 0);
  //pad input to a multiple of simdWidth
  while (nx & (simdWidth - 1)) {
    tx[nx++] = 0;
  }
  if (mp != nullptr) { // first mixer layer: feed results to second layer
    const size_t end = numContexts & ~1ULL;
    size_t i = 0;
    for (; i < end; i += 2) {
      int dp1 = 0;
      const int dp0 = dotProduct2(
        &wx[cxt[i + 0] * n],
        &wx[cxt[i + 1] * n], nx, dp1);
      pr[i + 0] = processDotProduct(mp, dp0, scaleFactor);
      pr[i + 1] = processDotProduct(mp, dp1, scaleFactor);
    }
    if (i < numContexts) {
      const int dp = dotProduct(&wx[cxt[i] * n], nx);
      pr[i] = processDotProduct(mp, dp, scaleFactor);
    }

    mp->set(0, 1);
    return mp->p();
  }
  else { // second (last) mixer layer: return prediction directly
    const int dp = scaleDotProduct(dotProduct(&wx[cxt[0] * n], nx), scaleFactor);
    return pr[0] = squash(dp);
  }
}

void Mixer::add(const int x) {
  assert(nx < n);
  assert(x == short(x));
  tx[nx++] = static_cast<short>(x);
}

void Mixer::set(const uint32_t cx, const uint32_t range) {
  assert(numContexts < s);
  assert(cx < range);
  assert(base + range <= m);
  cxt[numContexts++] = base + cx;
  base += range;
}

void Mixer::reset() {
  nx = 0;
  base = 0;
  numContexts = 0;
}
