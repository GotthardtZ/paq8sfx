#pragma once

#include "SystemDefines.h"

/**
 * A specialized string class: a NUL-terminated char array that grows as
 * needed. data[size - 1] is always the NUL.
 * String_init() makes a copy of a C string; free it with String_destroy().
 */
typedef struct String {
  char* data;
  uint64_t size; /**< used bytes, including the NUL terminator */
  uint64_t reserved; /**< allocated bytes */
} String;

void String_init(String* s, const char* text);
void String_destroy(String* s);
const char* String_c_str(const String* s);
/**
  * Does not include the NUL terminator
  * @return length of the string
  */
uint64_t String_strsize(const String* s);
void String_assign(String* s, const char* text);
void String_append(String* s, const char* text);
void String_appendChar(String* s, char c);
void String_appendInt(String* s, uint64_t x);
bool String_endsWith(const String* s, const char* ending);
void String_stripEnd(String* s, uint64_t count);
bool String_beginsWith(const String* s, const char* beginning);
void String_stripStart(String* s, uint64_t count);
int String_findLast(const String* s, char c);
