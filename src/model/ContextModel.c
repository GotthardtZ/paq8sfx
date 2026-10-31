#include "ContextModel.h"

void ContextModel_init(ContextModel* const cm, Shared* const sh) {
  cm->shared = sh;
  cm->contextModelGeneric = NULL;
  cm->selectedContextModel = NULL;
}

void ContextModel_destroy(ContextModel* const cm) {
  ContextModelGeneric_delete(cm->contextModelGeneric);
  cm->contextModelGeneric = NULL;
  cm->selectedContextModel = NULL;
}

int ContextModel_p(ContextModel* const cm) {
  Shared* const shared = cm->shared;
  INJECT_SHARED_bpos
  if( bpos == 0 ) {
    uint32_t* const blpos = &shared->State.blockPos;
    (*blpos)++;
    if (*blpos == 0) {
      if (cm->contextModelGeneric == NULL) {
        cm->contextModelGeneric = ContextModelGeneric_new(shared);
      }
      cm->selectedContextModel = cm->contextModelGeneric;
    }
  }

  return ContextModelGeneric_p(cm->selectedContextModel);

}
