#pragma once

#include "fileUtils.h"
#include "../String.h"

/**
 * A filename: a String with a few functions for paths.
 */
typedef String FileName;

int FileName_lastSlashPos(const FileName* name);
void FileName_keepFilename(FileName* name);
void FileName_keepPath(FileName* name);

/**
  * Prepare path string for screen output.
  */
void FileName_replaceSlashes(FileName* name);
