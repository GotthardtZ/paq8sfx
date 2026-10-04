/*
  PAQ8SFX – Experimental Self-Extracting Archive
  see README.md for information
  see CHANGELOG for version history
*/

//////////////////////// Versioning ////////////////////////////////////////

#define PROGNAME     "paq8sfx"
#define PROGVERSION  "1"  //update version here before publishing your changes
#define PROGYEAR     "2026"


#include "Utils.hpp"

#include <stdexcept>  //std::exception
#include <string> //std::stof, std::to_string

#include "Encoder.hpp"
#include "Shared.hpp"
#include "String.hpp"
#include "file/FileName.hpp"
#include "file/fileUtils2.hpp"
#include "filter/Filters.hpp"
#include "Models.hpp"
#include "PredictorMain.hpp"

typedef enum { DoNone, DoCompress, DoExtract } WHATTODO;

// ============================================================
//  SFX build
//  Supports three switches:
//    [no switch]      extract all payloads, run the AutoRun file
//    -d filename      decompress a single paq8sfx-compressed file
//    -a scriptfile    assemble a new SFX archive
//
//  Script file format (UTF-8 text, one directive per line):
//    //  <output filename, may contain spaces>   <- line 1: destination exe
//    //  <filename, may contain spaces>          <- line 2: AutoRun file (1st payload)
//    //  <filename, may contain spaces>          <- line 3+: further payloads
//    (blank lines are ignored, max MAX_FILE_ENTRIES payloads total)
//
//  In-memory per-entry record: filename + size only (no rowFlags).
//  Footer last byte == 0 means nothing no payload is appended.
//
//  Silence: define SFX_SILENT in the production build to strip all
//  diagnostic output.
// ============================================================
#ifdef SFX

#include "file/FileExeRead.hpp"

#ifndef _WIN32
#  include <unistd.h>
#  include <sys/stat.h>
#endif

#ifdef SFX_SILENT
#  define SFX_ERR(...)  ((void)0)
#  define SFX_MSG(...)  ((void)0)
#else
#  define SFX_ERR(...)  fprintf(stderr, __VA_ARGS__)
#  define SFX_MSG(...)  printf(__VA_ARGS__)
#endif

#define MAX_FILE_ENTRIES 8 // just a sensible upper limit

// ============================================================
//  Footer layout (appended after all payloads, all files raw):
//
//    [names block]  packed NUL-terminated filenames, listed order
//    [sizes block]  count × 4-byte LE uint32, listed order
//    count          1 byte
//    footerSize     1 byte  (total footer length incl. this byte;
//                            0 = no payload)
//
//  Access from end of file:
//    footerSize = raw[exeSize - 1]
//    count      = raw[exeSize - 2]
//    size[i]    = LE32(raw + exeSize - 2 - count*4 + i*4)
//    namesBase  = raw + exeSize - footerSize
// ============================================================

static uint32_t ReadLE32(const uint8_t* p) {
  return (uint32_t)p[0] |
    (uint32_t)p[1] << 8 |
    (uint32_t)p[2] << 16 |
    (uint32_t)p[3] << 24;
}

// ------------------------------------------------------------
//  Helper: append all bytes of srcPath to dst.
//  Returns bytes written, or 0 on error.
// ------------------------------------------------------------
static bool AppendFileSFX(FILE* dst, const char* srcPath, uint32_t* bytesWritten_out) {
  *bytesWritten_out = 0;

  FILE* src = fopen(srcPath, "rb");
  if (!src) {
    SFX_ERR("Cannot open: %s\n", srcPath);
    return false;
  }

  uint8_t buf[65536];
  size_t n;

  while ((n = fread(buf, 1, sizeof(buf), src)) > 0) {
    if (fwrite(buf, 1, n, dst) != n) {
      SFX_ERR("Write error: %s\n", srcPath);
      fclose(src);
      return false;
    }
    *bytesWritten_out += (uint32_t)n;
  }

  fclose(src);
  return true;
}

// ------------------------------------------------------------
//      Extract all payloads, chmod +x each (Linux), run
//      the AutoRun file (first listed payload).
// ------------------------------------------------------------
static void DoExtractAndRun() {
  FileExeRead exe;
  const uint8_t* raw = exe.buf;
  uint64_t       sz = exe.fileSize;

  uint8_t footerSize = raw[sz - 1];
  if (footerSize == 0) { SFX_ERR("No payload.\n"); exit(1); }

  uint8_t count = raw[sz - 2];
  if (count == 0 || count > MAX_FILE_ENTRIES) exit(1);

  const uint8_t* sizesBase = raw + sz - 2 - (uint64_t)count * 4;
  const uint8_t* namesBase = raw + sz - footerSize;
  const uint8_t* namesEnd = sizesBase;

  uint64_t totalPayload = 0;
  for (int i = 0; i < count; i++)
    totalPayload += ReadLE32(sizesBase + i * 4);

  uint64_t offset = sz - footerSize - totalPayload;

  const uint8_t* p = namesBase;
  char autorun[514] = { '.', '/' };
  bool executeAutoRun = true;

  for (int i = 0; i < count; i++) {
    const char* name = (const char*)p;
    while (p < namesEnd && *p) ++p;
    if (p >= namesEnd && i < count - 1) exit(1);
    ++p;

    uint32_t fsz = ReadLE32(sizesBase + i * 4);
    FILE* out = fopen(name, "wb");
    if (!out) exit(1);
    fwrite(raw + offset, 1, fsz, out);
    fclose(out);

#ifndef _WIN32
    chmod(name, 0755);
#endif
    if (i == 0) {
      if (name[0] == '\0') {
        SFX_ERR("AutoRun filename is empty, skipping execution.\n");
        executeAutoRun = false;
      }
      else if (fsz == 0) {
        SFX_ERR("AutoRun file has zero length, skipping execution.\n");
        executeAutoRun = false;
      }
      else {
        memcpy(autorun + 2, name, strnlen(name, 511));
      }
    }
    offset += fsz;
  }

  if (!executeAutoRun) return;

#ifdef _WIN32
  system(autorun + 2);
#else
  char* args[3] = { (char*)"/bin/sh", autorun, nullptr };
  execv("/bin/sh", args);
  exit(1);
#endif
}

// ------------------------------------------------------------
//  -d input output   Decompress a paq8sfx-compressed file.
// ------------------------------------------------------------
static void DoDecompress(const char* inFile, const char* outFile) {
  FileDisk archive;
  archive.open(inFile, /*readOnly=*/true);

  size_t pnLen = strlen(PROGNAME);
  for (size_t i = 0; i < pnLen; i++) {
    int c = archive.getchar();
    if (c == EOF || (unsigned char)c != (unsigned char)PROGNAME[i]) {
      SFX_ERR("%s: not a valid %s file.\n", inFile, PROGNAME);
      exit(1);
    }
  }
  int level = archive.getchar();
  if (level == EOF || level < 1 || level > 12) exit(1);

  Shared shared;
  shared.init(level);
  PredictorMain predictorMain(&shared);
  Encoder en(&predictorMain, DECOMPRESS, &archive);
  decompressFile(&shared, outFile, FMode::FDECOMPRESS, en);
  archive.close();
}

// ------------------------------------------------------------
//  -a scriptfile   Assemble a new SFX archive.
//
//  Script (read in binary mode):
//    line 1:   output path  (discarded here; caller used it)
//    line 2+:  one filename per line  → names block (raw bytes,
//              \r stripped, \n → \0)
//
//  The names block is written to the footer as-is.
//  Blank lines (empty names after stripping) are skipped.
// ------------------------------------------------------------
static void DoAssemble(const char* scriptFile) {
  FILE* f = fopen(scriptFile, "r");
  if (!f) { SFX_ERR("Cannot open script: %s\n", scriptFile); return; }

  char    outputPath[512], line[256];
  uint8_t names[256];
  int     namesLen = 0, count = 0;

  // Line 1: output path
  if (!fgets(outputPath, sizeof(outputPath), f)) { fclose(f); return; }
  outputPath[strcspn(outputPath, "\r\n")] = '\0';

  // Remaining lines: one filename each
  while (count < MAX_FILE_ENTRIES && fgets(line, sizeof(line), f)) {
    int len = (int)strcspn(line, "\r\n");
    if (len == 0) continue;
    if (namesLen + len + 1 > (int)sizeof(names)) {
      SFX_ERR("Footer too large.\n"); fclose(f); return;
    }
    memcpy(names + namesLen, line, len);
    namesLen += len;
    names[namesLen++] = '\0';
    count++;
  }
  fclose(f);

  if (count == 0) { SFX_ERR("No payload entries in script.\n"); return; }

  int footerSize = namesLen + count * 4 + 2;
  if (footerSize > 255) { SFX_ERR("Footer too large.\n"); return; }

  SFX_MSG("Assembling: %s  (%d file(s))\n", outputPath, count);

  // --- Locate stub in the running executable ---
  FileExeRead self;
  const uint8_t* selfRaw = self.buf;
  uint64_t       selfSize = self.fileSize;

  uint64_t stubEnd = selfSize;  // default: whole exe is stub
  {
    uint8_t fs = selfRaw[selfSize - 1];
    if (fs != 0) {
      uint8_t cnt = selfRaw[selfSize - 2];
      if (cnt > 0 && cnt <= MAX_FILE_ENTRIES) {
        uint64_t totalPay = 0;
        const uint8_t* sb = selfRaw + selfSize - 2 - (uint64_t)cnt * 4;
        for (int i = 0; i < cnt; i++) totalPay += ReadLE32(sb + i * 4);
        stubEnd = selfSize - fs - totalPay;
      }
    }
  }

  // --- Write stub ---
  FILE* out = fopen(outputPath, "wb");
  if (!out) { SFX_ERR("Cannot create: %s\n", outputPath); return; }
  if (fwrite(selfRaw, 1, (size_t)stubEnd, out) != (size_t)stubEnd) {
    SFX_ERR("Write error (stub).\n"); fclose(out); return;
  }
  fclose(out);
  SFX_MSG("Stub: %" PRIu64 " bytes\n", stubEnd);

  // --- Append files, collect sizes ---
  out = fopen(outputPath, "ab");
  if (!out) { SFX_ERR("Cannot open for appending: %s\n", outputPath); return; }

  uint32_t sizes[MAX_FILE_ENTRIES] = {};
  const uint8_t* np = names;
  for (int i = 0; i < count; i++) {
    const char* name = reinterpret_cast<const char*>(np);
    SFX_MSG("  [%d] %s\n", i, name);
    uint32_t sz;
    if (!AppendFileSFX(out, name, &sz)) {
      fclose(out);
      return;
    }
    sizes[i] = sz;
    while (*np) np++;
    np++;  // skip NUL
  }

  // --- Write footer ---
  // names block
  fwrite(names, 1, (size_t)namesLen, out);
  // sizes block (listed order, LE)
  for (int i = 0; i < count; i++) {
    uint8_t s[4] = {
      (uint8_t)(sizes[i] & 0xFF),
      (uint8_t)((sizes[i] >> 8) & 0xFF),
      (uint8_t)((sizes[i] >> 16) & 0xFF),
      (uint8_t)((sizes[i] >> 24) & 0xFF)
    };
    fwrite(s, 1, 4, out);
  }
  uint8_t tail[2] = { (uint8_t)count, (uint8_t)footerSize };
  fwrite(tail, 1, 2, out);
  fclose(out);

#ifndef _WIN32
  chmod(outputPath, 0755);
#endif

  SFX_MSG("Done: %s\n", outputPath);
}

// ------------------------------------------------------------
//  SFX main()
// ------------------------------------------------------------
int main(int argc, char** argv) {
  if (argc == 1 || (argc == 2 && strcmp(argv[1], "-x") == 0)) {
    DoExtractAndRun();
    return 0;
  }
  if (argc == 4 && strcmp(argv[1], "-d") == 0) {
    DoDecompress(argv[2], argv[3]);
    return 0;
  }
  if (argc == 3 && strcmp(argv[1], "-a") == 0) {
    DoAssemble(argv[2]);
    return 0;
  }
  SFX_ERR("Usage:\n"
    "  %s [no options]\n"
    "  %s -d <input> <output>\n"
    "  %s -a <scriptfile>\n",
    argv[0], argv[0], argv[0]);
  return 1;
}

#endif  // SFX


// ============================================================
//  FULL build  –  compress + decompress
// ============================================================
#ifdef FULL

int CompressExtract(int argc, char** argv) {
  Shared shared;

  printf(PROGNAME " v" PROGVERSION " (" PROGYEAR ")\n");

  WHATTODO whattodo = DoNone;
  int level = 0;

  FileName input;
  FileName output;
  FileName inputPath;
  FileName outputPath;
  FileName archiveName;

  for (int i = 1; i < argc; i++) {
    int argLen = static_cast<int>(strlen(argv[i]));
    if (argv[i][0] == '-') {
      if (argLen == 1) {
        quit("Empty command.");
      }
      if (argv[i][1] >= '0' && argv[i][1] <= '9') {
        if (whattodo != DoNone) {
          quit("Only one command may be specified.");
        }
        level = argv[i][1] - '0';
        if (argLen >= 3 && argv[i][2] >= '0' && argv[i][2] <= '9') {
          level = level * 10 + argv[i][2] - '0';
        }
        if (level < 1 || level > 12) {
          quit("Compression level must be between 1 and 12.");
        }
        whattodo = DoCompress;
      }
      else if (strcasecmp(argv[i], "-d") == 0) {
        if (whattodo != DoNone) {
          quit("Only one command may be specified.");
        }
        whattodo = DoExtract;
      }
      else {
        printf("Invalid command: %s", argv[i]);
        quit();
      }
    }
    else {
      if (input.strsize() == 0) {
        input += argv[i];
        input.replaceSlashes();
      }
      else if (output.strsize() == 0) {
        output += argv[i];
        output.replaceSlashes();
      }
      else {
        quit("More than two file names specified. Only an input and an output is needed.");
      }
    }
  }

  if (whattodo == DoNone) {
    quit("A command switch is required: -1..-12 to compress, -d to decompress.");
  }
  if (input.strsize() == 0) {
    printf("\nAn %s is required %s.\n",
      whattodo == DoCompress ? "input file" : "archive filename",
      whattodo == DoCompress ? "for compressing" : "for decompressing");
    quit();
  }

  int pathType = examinePath(input.c_str());
  if (pathType == 2 || pathType == 4) {
    printf("\nSpecified input is a directory but should be a file: %s", input.c_str());
    quit();
  }
  if (pathType == 3) {
    printf("\nSpecified input file does not exist: %s", input.c_str());
    quit();
  }
  if (pathType == 0) {
    printf("\nThere is a problem with the specified input file: %s", input.c_str());
    quit();
  }
  if (input.lastSlashPos() >= 0) {
    inputPath += input.c_str();
    inputPath.keepPath();
    input.keepFilename();
  }

  if (output.strsize() > 0) {
    pathType = examinePath(output.c_str());
    if (pathType == 1 || pathType == 3) {
      if (output.lastSlashPos() >= 0) {
        outputPath += output.c_str();
        outputPath.keepPath();
        output.keepFilename();
      }
    }
    else if (pathType == 2 || pathType == 4) {
      outputPath += output.c_str();
      if (!outputPath.endsWith("/") && !outputPath.endsWith("\\"))
        outputPath += GOODSLASH;
      output.resize(0);
      output.pushBack(0);
    }
    else {
      printf("\nThere is a problem with the specified output: %s", output.c_str());
      quit();
    }
  }

  if (whattodo == DoCompress) {
    archiveName += outputPath.c_str();
    if (output.strsize() == 0) {
      archiveName += input.c_str();
      archiveName += "." PROGNAME;
    }
    else {
      archiveName += output.c_str();
    }
  }
  else {
    archiveName += inputPath.c_str();
    archiveName += input.c_str();
  }

  Mode mode = whattodo == DoCompress ? COMPRESS : DECOMPRESS;

  FileName fn(inputPath.c_str());
  fn += input.c_str();
  getFileSize(fn.c_str());

  FileDisk archive;

  if (mode == DECOMPRESS) {
    archive.open(archiveName.c_str(), true);
    int len = static_cast<int>(strlen(PROGNAME));
    for (int i = 0; i < len; i++) {
      if (archive.getchar() != PROGNAME[i]) {
        printf("%s: not a valid %s file.", archiveName.c_str(), PROGNAME);
        quit();
      }
    }
    int c = archive.getchar();
    level = c;
    if (level > 12) quit("Unexpected compression level setting in archive.");
    if (c == EOF)   quit("Unexpected end of archive file.");
  }

  printf("\n");

  if (mode == COMPRESS) {
    printf("Creating archive %s ...\n", archiveName.c_str());
    archive.create(archiveName.c_str());
    archive.append(PROGNAME);
    archive.putChar(level);
  }

  if (whattodo == DoExtract && output.strsize() == 0) {
    output += input.c_str();
    const char* fileExtension = "." PROGNAME;
    if (output.endsWith(fileExtension)) {
      output.stripEnd(static_cast<int>(strlen(fileExtension)));
    }
    else {
      printf("Can't construct output filename from archive filename.\n"
        "Archive file extension must be: '%s'", fileExtension);
      quit();
    }
  }

  shared.init(level);

  PredictorMain predictorMain(&shared);
  Encoder en(&predictorMain, mode, &archive);
  uint64_t contentSize = 0;
  uint64_t totalSize = 0;

  if (mode == COMPRESS) {
    totalSize += en.size();  // header size

    FileName fn;
    fn += inputPath.c_str();
    fn += input.c_str();
    const char* fName = fn.c_str();
    uint64_t    fSize = getFileSize(fName);
    printf("\nFilename: %s (%" PRIu64 " bytes)\n", fName, fSize);
    compressfile(&shared, fName, fSize, en);
    totalSize += fSize + 4;  // 4: file size field
    contentSize += fSize;

    uint64_t preFlush = en.size();
    en.flush();
    totalSize += en.size() - preFlush;
    printf("-----------------------\n");
    printf("Total input size     : %" PRIu64 "\n", contentSize);
    printf("Total archive size   : %" PRIu64 "\n", en.size());
    printf("\n");
  }
  else {
    if (whattodo == DoExtract) {
      FileName fn;
      fn += outputPath.c_str();
      fn += output.c_str();
      decompressFile(&shared, fn.c_str(), FMode::FDECOMPRESS, en);
    }
  }

  archive.close();
  return 0;
}

int main(int argc, char** argv) {
  return CompressExtract(argc, argv);
}

#endif  // FULL
