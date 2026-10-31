#include "FileName.h"

int FileName_lastSlashPos(const FileName* const name) {
  int pos = String_findLast(name, '/');
  if( pos < 0 ) {
    pos = String_findLast(name, '\\');
  }
  return pos; //has no path when -1
}

void FileName_keepFilename(FileName* const name) {
  const int pos = FileName_lastSlashPos(name);
  String_stripStart(name, pos + 1); //don't keep last slash
}

void FileName_keepPath(FileName* const name) {
  const int pos = FileName_lastSlashPos(name);
  String_stripEnd(name, String_strsize(name) - pos - 1); //keep last slash
}

void FileName_replaceSlashes(FileName* const name) { //
  const uint64_t sSize = String_strsize(name);
  for( uint64_t i = 0; i < sSize; i++ ) {
    if(name->data[i] == BADSLASH ) {
      name->data[i] = GOODSLASH;
    }
  }
}
