#pragma once

#include "IPredictor.h"
#include "Shared.h"

typedef struct ErrorInfo
{
  uint32_t data[2], sum, mask, collected;
} ErrorInfo;

#define Mixer_MAX_LEARNING_RATE    (8 * 65536 - 1)
#define Mixer_MIN_LEARNING_RATE_S1 (2 * 65536 - 1)
#define Mixer_MIN_LEARNING_RATE_SN (6 * 65536 - 1)

typedef struct Mixer Mixer;

/**
  * Mixer combines models using @ref m neural networks with
  * @ref n inputs each, of which up to @ref s may be selected.  If s > 1 then
  * the outputs of these neural networks are combined using another
  * neural network (with arguments s, 1, 1). If s = 1 then the
  * output is direct.
  *
  * The SIMD implementations (Mixer_Scalar.c, Mixer_SSE2.c, Mixer_AVX2.c)
  * provide the three functions below; create a mixer with
  * MixerFactory_createMixer(), free it with Mixer_delete().
  */
struct Mixer {
  IPredictor predictor;
  const Shared* shared;
  size_t n; /**< max inputs */
  uint32_t m; /**< max contexts */
  uint32_t s; /**< max context sets */
  int scaleFactor; /**< scale factor for dot product */
  int lowerLimitOfLearningRate; /**< for linear learning rate decay */
  short* tx; /**< n inputs from add() */
  short* wx; /**< n*m weights */
  uint32_t* cxt; /**< s contexts */
  ErrorInfo* info; /**< stats for the adaptive learning rates  */
  int* rates; /**< learning rates */
  size_t numContexts; /**< number of contexts (0 to s)  */
  uint32_t base; /**< offset of next context */
  size_t nx; /**< number of inputs in tx, 0 to n */
  int* pr; /**< last result (scaled 12 bits) */
  Mixer* mp; /**< points to a second-layer Mixer to combine results, or NULL */
  int simdWidth; /**< number of shorts per SIMD lane, used for input padding */

  /**
    * Computes the dot product of input vector tx and weight vector @p w
    * over @p n elements.
    * @param w weight vector (aligned, padded to simdWidth)
    * @param n number of elements (multiple of simdWidth)
    * @return dot product (scaled by 1/256)
    */
  int (*dotProduct)(const Mixer* self, const short* w, size_t n);

  /**
    * Computes dot products of tx against two weight vectors simultaneously,
    * sharing tx loads across both.
    * @param w0 first weight vector
    * @param w1 second weight vector
    * @param n number of elements (multiple of simdWidth)
    * @param sum1 receives the dot product for w1
    * @return dot product for w0
    */
  int (*dotProduct2)(const Mixer* self, const short* w0, const short* w1, size_t n, int* sum1);

  /**
    * Adjusts weight vector @p w by adding a term proportional to
    * input vector tx and error @p e (gradient descent step).
    * @param w weight vector to update in place
    * @param n number of elements (multiple of simdWidth)
    * @param e scaled error term
    */
  void (*train)(const Mixer* self, short* w, size_t n, int e);
};

/**
  * Sets up the common part of a mixer; used by the SIMD implementations.
  * @param sh shared context
  * @param n max inputs (will be rounded up to a multiple of simdWidth)
  * @param m max contexts
  * @param s max context sets
  * @param simdWidth number of shorts per SIMD lane (controls input padding)
  */
void Mixer_init(Mixer* mixer, const Shared* sh, int n, int m, int s, int simdWidth);

/**
  * Frees a mixer created by MixerFactory_createMixer() (and its second layer).
  */
void Mixer_delete(Mixer* mixer);

/**
  * Predicts the next bit as a 12-bit probability (0 to 4095).
  * Subscribes to the UpdateBroadcaster on first call.
  * @return prediction
  */
int Mixer_p(Mixer* mixer);

void Mixer_setScaleFactor(Mixer* mixer, int sf0, int sf1);
void Mixer_setLowerLimitOfLearningRate(Mixer* mixer, int lr0, int lr1);
void Mixer_promote(Mixer* mixer, int x);

/**
  * Inputs x (call up to n times).
  * m.add(stretch(p)) inputs a prediction from one of n models.  The
  * prediction should be positive to predict a 1 bit, negative for 0,
  * nominally +-256 to +-2K.  The maximum allowed value is +-32K but
  * using such large values may cause overflow if n is large.
  * @param x
  */
static inline void Mixer_add(Mixer* const mixer, const int x) {
  assert(mixer->nx < mixer->n);
  assert(x == (short)x);
  mixer->tx[mixer->nx++] = (short)x;
}

/**
  * Selects @ref cx as one of @ref range neural networks to use.
  * 0 <= cx < range. Should be called up to @ref s times such
  * that the total of the ranges is <= @ref m.
  * @param cx
  * @param range
  */
void Mixer_set(Mixer* mixer, uint32_t cx, uint32_t range);

void Mixer_reset(Mixer* mixer);
