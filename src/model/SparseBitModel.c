#include "SparseBitModel.hpp"

SparseBitModel::SparseBitModel(const Shared* const sh, const uint64_t size) : shared(sh), cm(sh, size, nCM, 64) {}

void SparseBitModel::mix(Mixer &m) {

  INJECT_SHARED_bpos
  if (bpos == 0) {
    INJECT_SHARED_c4
    uint64_t i = 0;


    //the following context are beneficial for both text and binary files
    cm.set(hash(++i, c4 & 0x00f0f0ff));
    cm.set(hash(++i, c4 & 0xdfdfdfe0));
    cm.set(hash(++i, c4 & 0xffc0ffc0)); //note: silesia/osdb does not like this context
    cm.set(hash(++i, c4 & 0xe0ffffe0)); //note: silesia/osdb does not like this context
    cm.set(hash(++i, c4 & 0x00e0e0e0));
    cm.set(hash(++i, c4 & 0xe0e0e0e0));

    INJECT_SHARED_blockType

    cm.set(hash(++i, c4 & 0x0000f8f8));
    cm.set(hash(++i, c4 & 0x00f8f8f8));
    cm.set(hash(++i, c4 & 0xf8f8f8f8));
    cm.set(hash(++i, c4 & 0x0f0f0f0f));

    assert((int)i == nCM);
  }

  cm.mix(m);
}
