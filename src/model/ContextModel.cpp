#include "ContextModel.hpp"
#include "ContextModelGeneric.cpp"

ContextModel::ContextModel(Shared* const sh, Models* const models, const MixerFactory* const mf) :
  shared(sh), 
  models(models), 
  mixerFactory(mf)
{}

int ContextModel::p() {
  INJECT_SHARED_bpos
  if( bpos == 0 ) {
    uint32_t& blpos = shared->State.blockPos;
    blpos++;
    if (blpos == 0) {
      static ContextModelGeneric contextModelGeneric{ shared, models, mixerFactory };
      selectedContextModel = &contextModelGeneric;
    }
  }

  return selectedContextModel->p();

}
