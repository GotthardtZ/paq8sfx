#include "Shared.hpp"
#include "ArithmeticEncoder.hpp"
#include "Simd.hpp"

Shared::Shared() {
}

void Shared::init(uint8_t level) {

  this->level = level;
  mem = UINT64_C(65536) << level;
  uint32_t bufMem = static_cast<uint32_t>(min(mem * 8, UINT64_C(1) << 30)); /**< no reason to go over 1 GB */
  assert(isPowerOf2(bufMem));
  buf.setSize(bufMem);

  // Determine CPU's (and OS) support for SIMD vectorization instruction set
  int simdIset = simdDetect();

  // Set highest or user selected vectorization mode
  if (simdIset >= 9) {
    chosenSimd = SIMDType::SIMD_AVX2;
  }
  else if (simdIset >= 6) {
    chosenSimd = SIMDType::SIMD_SSE41;
  }
  else if (simdIset >= 4) {
    chosenSimd = SIMDType::SIMD_SSE3;
  }
  else if (simdIset >= 3) {
    chosenSimd = SIMDType::SIMD_SSE2;
  }
  else {
    chosenSimd = SIMDType::SIMD_NONE;
  }
}

void Shared::update(int y, uint32_t p, bool isMissed) {
  State.y = y;
  State.c0 += State.c0 + y;
  State.bitPosition = (State.bitPosition + 1) & 7;
  if(State.bitPosition == 0 ) {
    State.c1 = State.c0;
    buf.add(State.c1);
    State.c8 = (State.c8 << 8) | (State.c4 >> 24);
    State.c4 = (State.c4 << 8) | State.c0;
    State.c0 = 1;
  }
 
  State.misses = (State.misses << 1) | static_cast<uint32_t>(isMissed);

  constexpr uint32_t shift = ArithmeticEncoder::PRECISION - 6;
  constexpr uint32_t maxp = (1u << ArithmeticEncoder::PRECISION) - 1;

  State.loss = ((y == 0 ? p : maxp - p)) >> shift; //0..1023
  assert(State.loss >= 0 && State.loss <= 1023);

  // Broadcast to all current subscribers: y (and c0, c1, c4, etc) is known
  updateBroadcaster.broadcastUpdate();
}

void Shared::reset() {
  buf.reset();
  memset(&State, 0, sizeof(State));
  State.c0 = 1;
}

UpdateBroadcaster *Shared::GetUpdateBroadcaster() const {
  UpdateBroadcaster* updater = const_cast<UpdateBroadcaster*>(&updateBroadcaster);
  return updater;
}

