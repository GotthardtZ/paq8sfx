#pragma once

#include "BlockType.h"
#include "RingBuffer.h"
#include "SIMDType.h"
#include "UpdateBroadcaster.h"

// helper #defines to access shared variables
#define INJECT_SHARED_buf   const RingBuffer* const buf = &shared->buf;
#define INJECT_SHARED_pos   const uint32_t pos = RingBuffer_getpos(&shared->buf);
#define INJECT_SHARED_y     const uint8_t  y = shared->State.y;
#define INJECT_SHARED_c0    const uint8_t  c0 = shared->State.c0;
#define INJECT_SHARED_c1    const uint8_t  c1 = shared->State.c1;
#define INJECT_SHARED_bpos  const uint8_t  bpos = shared->State.bitPosition;
#define INJECT_SHARED_c4    const uint32_t c4 = shared->State.c4;
#define INJECT_SHARED_c8    const uint32_t c8 = shared->State.c8;

#define INJECT_SHARED_blockType const BlockType blockType = shared->State.blockType;
#define INJECT_SHARED_blockPos const uint32_t blockPos = shared->State.blockPos;

/**
 * Shared information by all the models and some other classes.
 */
typedef struct Shared {
  UpdateBroadcaster updateBroadcaster; /**< use Shared_getUpdateBroadcaster() */

  //Shared state and statistics (global)

  RingBuffer buf; /**< Rotating input queue set by Predictor */
  SIMDType chosenSimd; /**< set by the CPU dispatcher */
  uint8_t level; /**< level=0: no compression (only transformations), level=1..12 compress using less..more RAM */
  uint64_t mem; /**< pre-calculated value of 65536 * 2^level */

  struct {

    //
    // Global state, used by most models, updated after every bit by update(y)
    // 

    uint8_t y; /**< Last bit, 0 or 1 */
    uint8_t c0; /**< Last 0-7 bits of the partial byte with a leading 1 bit (1-255) */
    uint8_t c1; /**< Last whole byte, equals to c4&0xff or buf(1) */
    uint8_t bitPosition; /**< Bits in c0 (0 to 7), in other words the position of the bit to be predicted (0=MSB) */
    uint32_t c4; /**< Last 4 whole bytes (buf(4)..buf(1)), packed.  Last byte is bits 0-7. */
    uint32_t c8; /**< Another 4 bytes (buf(8)..buf(5)) */
    uint32_t misses; //updated by the Predictor, used by SSE stage
    uint32_t loss;

    //written by BlockModel, used by many models 
    //see PredictorMain, ContextModel
    BlockType blockType; //current blockType
    int blockInfo;       //current block info (or -1 when none)
    uint32_t blockPos;   //relative position in block

    //
    // State and statistics per model - set by the individual models
    // Order in appearance: models may use information from models that appears above them
    //

    //MatchModel
    struct {
      uint8_t length2;      //used by SSE stage and RecordModel
      uint8_t mode3;        //used by SSE stage 
      uint8_t mode5;        //used by SSE stage 
      uint16_t expectedByte; //used by SSE stage and RecordModel; 0-256 where 256 = not valid
    } Match;

    //NormalModel
    struct {
      uint8_t order; // 0-7
      uint64_t cxt[15]; // context hashes used by NormalModel and MatchModel
    } NormalModel;

    //ExeModel
    struct {
      uint8_t state; // used by SSE stage
    } x86_64;

  } State;
} Shared;

/**
 * Sets up a Shared for the given compression level (1..12): clears it,
 * allocates the history buffer and selects the SIMD code path.
 * Also builds the lookup tables used by the models (stretch, ilog).
 */
void Shared_init(Shared* shared, uint8_t level);

/**
 * Frees the history buffer.
 */
void Shared_destroy(Shared* shared);

void Shared_update(Shared* shared, int y, uint32_t p, bool isMissed);
void Shared_reset(Shared* shared);

/**
 * Predictors subscribe through a const Shared, so the broadcaster is
 * handed out as non-const.
 */
static inline UpdateBroadcaster* Shared_getUpdateBroadcaster(const Shared* const shared) {
  return (UpdateBroadcaster*)&shared->updateBroadcaster;
}
