#include "ContextModel.hpp"
#include "ContextModelGeneric.cpp"

ContextModel::ContextModel(Shared* const sh, const MixerFactory* const mf) :
  shared(sh),
  mixerFactory(mf)
{}

ContextModel::~ContextModel() {
  delete contextModelGeneric;
}

int ContextModel::p() {
  INJECT_SHARED_bpos
  if( bpos == 0 ) {
    uint32_t& blpos = shared->State.blockPos;
    blpos++;
    if (blpos == 0) {
      if (contextModelGeneric == nullptr) {
        contextModelGeneric = new ContextModelGeneric(shared, mixerFactory);
      }
      selectedContextModel = contextModelGeneric;
    }
  }

  return selectedContextModel->p();

}
