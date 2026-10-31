#include "String.h"
#include "Array.h"
#include "Utils.h"

#ifdef NDEBUG
  #define chk_consistency(s) ((void) 0)
#else
  static void chk_consistency(const String* const s) {
    for (uint64_t i = 0; i < s->size - 1; i++) {
      if (s->data[i] == 0) {
        quit("Internal error - string consistency check failed (1).");
      }
    }
    if (s->data[s->size - 1] != 0) {
      quit("Internal error - string consistency check failed (2).");
    }
  }
#endif

// Sets the used size to newSize bytes (the NUL included), growing the
// allocated memory if needed; the content is kept.
static void String_resize(String* const s, const uint64_t newSize) {
  if (newSize > s->reserved) {
    uint64_t newReserved = s->reserved * 2 + 16;
    if (newReserved < newSize) {
      newReserved = newSize;
    }
    char* const newData = (char*)Array_new(newReserved, 1, 16);
    if (s->size > 0) {
      memcpy(newData, s->data, (size_t)s->size);
    }
    Array_delete(s->data);
    s->data = newData;
    s->reserved = newReserved;
  }
  s->size = newSize;
}

static void String_pushBack(String* const s, const char c) {
  String_resize(s, s->size + 1);
  s->data[s->size - 1] = c;
}

static void appendIntRecursive(String* const s, uint64_t x) {
  if( x <= 9 ) {
    String_pushBack(s, (char)('0' + x));
  } else {
    const uint64_t rem = x % 10;
    x = x / 10;
    if( x != 0 ) {
      appendIntRecursive(s, x);
    }
    String_pushBack(s, (char)('0' + rem));
  }
}

void String_init(String* const s, const char* const text) {
  s->data = NULL;
  s->size = 0;
  s->reserved = 0;
  String_assign(s, text);
}

void String_destroy(String* const s) {
  Array_delete(s->data);
  s->data = NULL;
  s->size = 0;
  s->reserved = 0;
}

const char* String_c_str(const String* const s) {
  return s->data;
}

uint64_t String_strsize(const String* const s) {
  chk_consistency(s);
  assert(s->size > 0);
  return s->size - 1;
}

void String_assign(String* const s, const char* const text) {
  String_resize(s, strlen(text) + 1);
  memcpy(s->data, text, (size_t)s->size);
  chk_consistency(s);
}

void String_append(String* const s, const char* const text) {
  const uint64_t pos = s->size;
  const uint64_t len = strlen(text);
  String_resize(s, pos + len);
  memcpy(&s->data[pos - 1], text, (size_t)(len + 1));
  chk_consistency(s);
}

void String_appendChar(String* const s, const char c) {
  s->size--; //Remove NUL
  String_pushBack(s, c);
  String_pushBack(s, 0); //Append NUL
  chk_consistency(s);
}

void String_appendInt(String* const s, const uint64_t x) {
  s->size--; //Remove NUL
  if( x == 0 ) {
    String_pushBack(s, '0');
  } else {
    appendIntRecursive(s, x);
  }
  String_pushBack(s, 0); //Append NUL
  chk_consistency(s);
}

bool String_endsWith(const String* const s, const char* const ending) {
  const uint64_t endingSize = strlen(ending);
  if( endingSize > String_strsize(s)) {
    return false;
  }
  const int cmp = memcmp(ending, &s->data[String_strsize(s) - endingSize], (size_t)endingSize);
  return (cmp == 0);
}

void String_stripEnd(String* const s, const uint64_t count) {
  assert(String_strsize(s) >= count);
  const uint64_t newSize = String_strsize(s) - count;
  String_resize(s, newSize);
  String_pushBack(s, 0); //Append NUL
  chk_consistency(s);
}

bool String_beginsWith(const String* const s, const char* const beginning) {
  const uint64_t beginningSize = strlen(beginning);
  if( beginningSize > String_strsize(s)) {
    return false;
  }
  const int cmp = memcmp(beginning, &s->data[0], (size_t)beginningSize);
  return (cmp == 0);
}

void String_stripStart(String* const s, const uint64_t count) {
  assert(String_strsize(s) >= count);
  const uint64_t newSize = String_strsize(s) - count;
  memmove(&s->data[0], &s->data[count], (size_t)newSize);
  String_resize(s, newSize);
  String_pushBack(s, 0); //Append NUL
  chk_consistency(s);
}

int String_findLast(const String* const s, const char c) {
  uint64_t i = String_strsize(s);
  while( i-- > 0 ) {
    if(s->data[i] == c ) {
      return (int)i;
    }
  }
  return -1; //not found
}
