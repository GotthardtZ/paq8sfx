#pragma once

#include "../ContextMap2.h"
#include "../IndirectMap.h"
#include "../Mixer.h"
#include "../Shared.h"

/**
 * Model for x86/x64 code.
 * Based on the previous paq* exe models and on DisFilter (http://www.farbrausch.de/~fg/code/disfilter/) by Fabian Giesen.
 *
 * Attempts to parse the input stream as x86/x64 instructions, and quantizes them into 32-bit representations that are then
 * used as context to predict the next bits, and also extracts possible sparse contexts at several previous positions that
 * are relevant to parsing (prefixes, opcode, Mod and R/m fields of the ModRM byte, Scale field of the SIB byte)
 *
 * The model's state is private to ExeModel.c: create it with ExeModel_new(),
 * free it with ExeModel_delete().
 */
enum {
  ExeModel_nCM1 = 10,
  ExeModel_nCM2 = 10,
  ExeModel_nIM = 1,
  ExeModel_MIXERINPUTS =
          (ExeModel_nCM1 + ExeModel_nCM2) * (ContextMap2_MIXERINPUTS) +
          ExeModel_nIM * IndirectMap_MIXERINPUTS, /**< 142 */
  ExeModel_MIXERCONTEXTS = 1024 + 1024 + 1024 + 8192 + 8192 + 8192, /**< 27648 */
  ExeModel_MIXERCONTEXTSETS = 6
};

typedef struct ExeModel ExeModel;

ExeModel* ExeModel_new(Shared* sh, uint64_t size);
void ExeModel_delete(ExeModel* em);
void ExeModel_mix(ExeModel* em, Mixer* m);
