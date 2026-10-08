#include "PredictorMain.hpp"
#include "ArithmeticEncoder.hpp"

PredictorMain::PredictorMain(Shared* const sh) : shared(sh), sse(sh) {
  shared->reset();
  mixerFactory = new MixerFactory(shared);
  contextModel = new ContextModel(sh, mixerFactory);
}

PredictorMain::~PredictorMain() {
  delete contextModel;
  delete mixerFactory;
}

uint32_t PredictorMain::p() {
  uint32_t pr = contextModel->p();
  pr = sse.p(pr);
  return pr;
}
