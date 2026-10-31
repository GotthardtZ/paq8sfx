#pragma once

#include "../SystemDefines.h"
#include <sys/stat.h>
#include <sys/types.h>

//////////////////// IO functions ///////////////////////////////
// File names are UTF-8 (char*) everywhere in this program. Linux takes them
// as they are; Windows needs UTF-16 (wchar_t*), so the wrappers below convert
// them before calling the Windows functions.


// The preferred slash for displaying
// We will change the BADSLASH to GOODSLASH before displaying a path string to the user
#ifdef WINDOWS
#define BADSLASH '/'
#define GOODSLASH '\\'
#else
#define BADSLASH '\\'
#define GOODSLASH '/'
#endif

#define FILE_READ  0
#define FILE_WRITE 1

/**
 * Wrapper function (Linux vs Windows) to open a file
 * @param filename
 * @param mode FILE_READ: open an existing file for reading;
 *             FILE_WRITE: create the file (or empty it) for reading and writing
 * @return the file, or NULL on failure
 */
FILE* openFile(const char* filename, int mode);

/**
 * examines given "path" and returns:
 * 0: on error
 * 1: when exists and is a file
 * 2: when exists and is a directory
 * 3: when does not exist, but looks like a file       /abcd/efgh
 * 4: when does not exist, but looks like a directory  /abcd/efgh/
 */
int examinePath(const char* path);

/**
 * Creates a directory if it does not exist
 * @return 0: failed, 1: created successfully, 2: the directory already exists
 */
int makeDir(const char* dir);

/**
 * Creates the directories in the path of filename if they don't exist.
 */
void makeDirectories(const char* filename);
