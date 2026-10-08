/*
  PAQ8SFX – Experimental Self-Extracting Archive
  see README.md for information
  see CHANGELOG for version history
*/

//////////////////////// Versioning ////////////////////////////////////////

#define PROGNAME     "paq8sfx"
#define PROGVERSION  "2"  //update version here before publishing your changes
#define PROGYEAR     "2026"


#include "Utils.hpp"
#include "Encoder.hpp"
#include "Shared.hpp"
#include "String.hpp"
#include "file/FileExeRead.hpp"
#include "file/FileMemRead.hpp"
#include "file/FileName.hpp"
#include "file/fileUtils2.hpp"
#include "filter/Filters.hpp"
#include "PredictorMain.hpp"

#ifdef UNIX
#include <sys/stat.h> //chmod()
#endif

// This file is compiled twice (see SystemDefines.hpp):
//   FULL   paq8sfx, the compressor:  compress, decompress, assemble a package
//   SFX    the stub:                 extract a package, decompress, assemble
//                                    a package (an extract-only stub only extracts)
//
// Messages: paq8sfx always reports what it is doing. The stub reports only
// if SFX_SILENT is not defined: SFX_ERR for errors, SFX_MSG for the rest.

#if defined(FULL)
#  define SFX_ERR(...)  printf(__VA_ARGS__)
#  define SFX_MSG(...)  printf(__VA_ARGS__)
#elif defined(SFX_SILENT)
#  define SFX_ERR(...)  ((void)0)
#  define SFX_MSG(...)  ((void)0)
#else
#  define SFX_ERR(...)  fprintf(stderr, __VA_ARGS__)
#  define SFX_MSG(...)  printf(__VA_ARGS__)
#endif


// ============================================================
//  Archives
//
//  An archive (.paq8sfx) holds one compressed file:
//    the text "paq8sfx", the compression level (1 byte),
//    then the compressed data.
// ============================================================

static const char ARCHIVE_EXTENSION[] = "." PROGNAME;

// Reads the start of an archive.
// Returns the compression level, or 0 if this is not a paq8sfx archive.
static int ReadArchiveHeader(File* archive) {
  for (const char* expected = PROGNAME; *expected != '\0'; expected++) {
    if (archive->getchar() != static_cast<uint8_t>(*expected)) return 0;
  }
  const int level = archive->getchar();
  return (level >= 1 && level <= 12) ? level : 0;
}


// ============================================================
//  Packages
//
//  A package is a stub with payload files and a footer appended.
//  Nothing in it is compressed by this code: a compressed payload
//  is an archive that was made by paq8sfx beforehand.
//
//    [stub] [payload 1] ... [payload n] [names] [sizes] [count] [footer size]
//
//    names        the payload file names in listed order, each
//                 followed by a zero byte
//    sizes        the payload sizes in listed order, 4 bytes each,
//                 least significant byte first
//    count        number of payloads, 1 byte
//    footer size  size of names + sizes + count + footer size,
//                 1 byte; 0 means "no payload" (a bare stub)
//
//  A package is assembled from a script file, a text file with one
//  file name per line (blank lines are ignored):
//    line 1:   the package to create
//    line 2:   the first payload. The full stub runs it after
//              extraction (the AutoRun file); for an extract-only
//              stub it is a payload like the others.
//    line 3+:  further payloads, at most MAX_PAYLOADS in total
// ============================================================

#define MAX_PAYLOADS    8    // just a sensible upper limit
#define MAX_FOOTER_SIZE 255  // the footer size is stored in one byte

static uint32_t ReadLE32(const uint8_t* p) {
  return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static void WriteLE32(uint8_t* p, uint32_t value) {
  p[0] = (uint8_t)value;
  p[1] = (uint8_t)(value >> 8);
  p[2] = (uint8_t)(value >> 16);
  p[3] = (uint8_t)(value >> 24);
}

struct Payload {
  const char*    name;
  const uint8_t* data;
  uint32_t       size;
};

// A package, as found in memory. The names and the data are not copied:
// they point into the package image.
struct Package {
  uint64_t stubSize;                // size of the stub part
  int      count;                   // number of payloads; 0 for a bare stub
  Payload  payloads[MAX_PAYLOADS];
};

// Locates the payloads of a package image from its footer.
// An image whose last bytes do not form a valid footer is a bare stub:
// it has no payloads, and the whole image is the stub.
static void ParsePackage(const uint8_t* image, uint64_t imageSize, Package& package) {
  package.stubSize = imageSize;
  package.count = 0;
  if (imageSize < 2) return;

  const uint32_t footerSize = image[imageSize - 1];
  const uint32_t count = image[imageSize - 2];
  // Every payload has a name of at least one character, its zero byte and 4 size bytes.
  if (count == 0 || count > MAX_PAYLOADS || footerSize < count * 6 + 2 || footerSize > imageSize) return;

  const uint8_t* const sizes = image + imageSize - 2 - count * 4;
  const uint8_t* name = image + imageSize - footerSize;
  uint64_t payloadsSize = 0;
  for (uint32_t i = 0; i < count; i++) {
    const uint8_t* nameEnd = name;
    while (nameEnd < sizes && *nameEnd != 0) nameEnd++;
    if (nameEnd == name || nameEnd == sizes) return;  // empty, or not ended by a zero byte
    package.payloads[i].name = reinterpret_cast<const char*>(name);
    package.payloads[i].size = ReadLE32(sizes + i * 4);
    payloadsSize += package.payloads[i].size;
    name = nameEnd + 1;
  }
  if (name != sizes || payloadsSize > imageSize - footerSize) return;

  package.stubSize = imageSize - footerSize - payloadsSize;
  const uint8_t* data = image + package.stubSize;
  for (uint32_t i = 0; i < count; i++) {
    package.payloads[i].data = data;
    data += package.payloads[i].size;
  }
  package.count = static_cast<int>(count);
}

#if defined(FULL) || !defined(SFX_EXTRACT_ONLY)

// Appends the content of a file to the package.
// Returns false on error; otherwise size is the number of bytes appended.
static bool AppendFile(FILE* package, const char* path, uint32_t& size) {
  FILE* file = fopen(path, "rb");
  if (!file) {
    SFX_ERR("Cannot open: %s\n", path);
    return false;
  }

  // Not on the stack: in the libc-free stub a function may not use more than
  // 4 KB of stack (see "The libc-free stub" in README.md).
  static uint8_t buffer[65536];
  uint64_t total = 0;
  bool ok = true;
  size_t n;
  while (ok && (n = fread(buffer, 1, sizeof(buffer), file)) > 0) {
    total += n;
    if (total > UINT32_MAX) {
      SFX_ERR("File too large (4 GB or more): %s\n", path);
      ok = false;
    }
    else if (fwrite(buffer, 1, n, package) != n) {
      SFX_ERR("Write error: %s\n", path);
      ok = false;
    }
  }
  fclose(file);
  size = static_cast<uint32_t>(total);
  return ok;
}

// Assembles a new package: the stub part of stubImage (if stubImage is a
// package itself, its payloads are left out), the files listed in the script
// file, and the footer.
// Returns false on error.
static bool Assemble(const uint8_t* stubImage, uint64_t stubImageSize, const char* scriptFile) {
  FILE* script = fopen(scriptFile, "rb");
  if (!script) {
    SFX_ERR("Cannot open script: %s\n", scriptFile);
    return false;
  }

  // Line 1: the package to create.
  char packagePath[512];
  if (!fgets(packagePath, sizeof(packagePath), script)) {
    SFX_ERR("Empty script.\n");
    fclose(script);
    return false;
  }
  packagePath[strcspn(packagePath, "\r\n")] = '\0';

  // The other lines: the payload names. They are collected at the start of
  // the footer, exactly as they will be stored.
  uint8_t footer[MAX_FOOTER_SIZE];
  int namesSize = 0;
  int count = 0;
  char line[256];
  bool ok = true;
  while (ok && fgets(line, sizeof(line), script)) {
    const int length = static_cast<int>(strcspn(line, "\r\n"));
    if (length == 0) continue;
    if (count == MAX_PAYLOADS) {
      SFX_ERR("Too many files in the script, at most %d are possible.\n", MAX_PAYLOADS);
      ok = false;
    }
    else if (namesSize + (length + 1) + (count + 1) * 4 + 2 > MAX_FOOTER_SIZE) {
      SFX_ERR("The file names in the script are too long in total.\n");
      ok = false;
    }
    else {
      memcpy(footer + namesSize, line, length);
      footer[namesSize + length] = '\0';
      namesSize += length + 1;
      count++;
    }
  }
  fclose(script);
  if (!ok) return false;
  if (count == 0) {
    SFX_ERR("No files listed in the script.\n");
    return false;
  }

  SFX_MSG("Assembling: %s  (%d file(s))\n", packagePath, count);

  FILE* out = fopen(packagePath, "wb");
  if (!out) {
    SFX_ERR("Cannot create: %s\n", packagePath);
    return false;
  }

  // The stub
  Package stub;
  ParsePackage(stubImage, stubImageSize, stub);
  ok = fwrite(stubImage, 1, static_cast<size_t>(stub.stubSize), out) == stub.stubSize;
  SFX_MSG("Stub: %" PRIu64 " bytes\n", stub.stubSize);

  // The payloads; their sizes go into the footer, after the names.
  uint8_t* const sizes = footer + namesSize;
  const char* name = reinterpret_cast<const char*>(footer);
  for (int i = 0; ok && i < count; i++) {
    SFX_MSG("  [%d] %s\n", i, name);
    uint32_t size = 0;
    ok = AppendFile(out, name, size);
    WriteLE32(sizes + i * 4, size);
    name += strlen(name) + 1;
  }

  // The footer
  const int footerSize = namesSize + count * 4 + 2;
  footer[footerSize - 2] = static_cast<uint8_t>(count);
  footer[footerSize - 1] = static_cast<uint8_t>(footerSize);
  ok = ok && fwrite(footer, 1, footerSize, out) == static_cast<size_t>(footerSize);
  fclose(out);
  if (!ok) {
    SFX_ERR("Cannot write: %s\n", packagePath);
    return false;
  }

#ifdef UNIX
  chmod(packagePath, 0755);  // a package is an executable
#endif

  SFX_MSG("Done: %s\n", packagePath);
  return true;
}

#endif  // FULL || !SFX_EXTRACT_ONLY


// ============================================================
//  SFX build (the stub)
// ============================================================
#ifdef SFX

// Decompresses an archive to outFile. Returns 0 on success, 1 on error.
static int DecompressArchive(File* archive, const char* outFile) {
  const int level = ReadArchiveHeader(archive);
  if (level == 0) {
    SFX_ERR("Not a valid %s archive.\n", PROGNAME);
    return 1;
  }

  // Shared and PredictorMain hold the whole state of the model. Each archive
  // gets a new pair, so several archives can be decompressed in a row.
  // Shared is too large for the stack of the libc-free stub (see AppendFile).
  Shared* shared = new Shared();
  shared->init(level);
  {
    PredictorMain predictorMain(shared);
    Encoder en(&predictorMain, DECOMPRESS, archive);
    decompressFile(shared, outFile, FMode::FDECOMPRESS, en);
  }
  delete shared;
  return 0;
}

// Writes a payload to a file in the current folder. Returns false on error.
static bool WritePayload(const Payload& payload) {
  FILE* out = fopen(payload.name, "wb");
  if (!out) {
    SFX_ERR("Cannot create: %s\n", payload.name);
    return false;
  }
  const bool ok = fwrite(payload.data, 1, payload.size, out) == payload.size;
  fclose(out);
  if (!ok) SFX_ERR("Write error: %s\n", payload.name);
  return ok;
}

// Makes an extracted file executable (Linux; nothing to do on Windows).
static void MakeExecutable(const char* path) {
#ifdef UNIX
  chmod(path, 0755);
#else
  (void)path;
#endif
}

#ifndef SFX_EXTRACT_ONLY

// ------------------------------------------------------------
//  The full stub
//    package [-x]             extract all payloads, run the first one
//    package -d input output  decompress an archive
//    package -a scriptfile    assemble a new package from this one
// ------------------------------------------------------------

// Extracts all payloads and runs the first one, the AutoRun file.
static int ExtractAndRun() {
  FileExeRead self;
  Package package;
  ParsePackage(self.buf, self.fileSize, package);
  if (package.count == 0) {
    SFX_ERR("No payload.\n");
    return 1;
  }

  for (int i = 0; i < package.count; i++) {
    if (!WritePayload(package.payloads[i])) return 1;
    MakeExecutable(package.payloads[i].name);
  }

  const Payload& autoRun = package.payloads[0];
  if (autoRun.size == 0) {
    SFX_ERR("AutoRun file has zero length, skipping execution.\n");
    return 0;
  }

#ifdef WINDOWS
  // The command interpreter runs it: a .cmd or .bat file, or a program.
  system(autoRun.name);
  return 0;
#else
  // The shell runs it; this program becomes the shell. The name gets "./"
  // in front: a plain name would be searched for on the PATH.
  char path[2 + MAX_FOOTER_SIZE] = "./";
  memcpy(path + 2, autoRun.name, strlen(autoRun.name) + 1);
  char* args[3] = { const_cast<char*>("/bin/sh"), path, nullptr };
  execv("/bin/sh", args);
  return 1;  // only reached if the shell could not be started
#endif
}

static int DecompressFile(const char* archiveFile, const char* outFile) {
  FileDisk archive;
  archive.open(archiveFile, /*mustSucceed=*/true);
  const int result = DecompressArchive(&archive, outFile);
  archive.close();
  return result;
}

int main(int argc, char** argv) {
  if (argc == 1 || (argc == 2 && strcmp(argv[1], "-x") == 0)) {
    return ExtractAndRun();
  }
  if (argc == 4 && strcmp(argv[1], "-d") == 0) {
    return DecompressFile(argv[2], argv[3]);
  }
  if (argc == 3 && strcmp(argv[1], "-a") == 0) {
    FileExeRead self;
    return Assemble(self.buf, self.fileSize, argv[2]) ? 0 : 1;
  }
  SFX_ERR("Usage:\n"
    "  %s [no options]\n"
    "  %s -d <input> <output>\n"
    "  %s -a <scriptfile>\n",
    argv[0], argv[0], argv[0]);
  return 1;
}

#else  // SFX_EXTRACT_ONLY

// ------------------------------------------------------------
//  The extract-only stub
//    package [-x]   extract all payloads; a payload named
//                   *.paq8sfx is decompressed to the name without
//                   that extension, the others are written as they are
// ------------------------------------------------------------

// If name ends in ".paq8sfx" (in any letter case): the length of the name
// without that extension. Otherwise 0.
static size_t LengthWithoutArchiveExtension(const char* name) {
  const size_t extensionLength = sizeof(ARCHIVE_EXTENSION) - 1;
  const size_t length = strlen(name);
  if (length <= extensionLength) return 0;
  const char* extension = name + length - extensionLength;
  for (size_t i = 0; i < extensionLength; i++) {
    char c = extension[i];
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    if (c != ARCHIVE_EXTENSION[i]) return 0;
  }
  return length - extensionLength;
}

// Extracts all payloads, decompressing the compressed ones straight from
// the package image in memory.
static int ExtractAll() {
  FileExeRead self;
  Package package;
  ParsePackage(self.buf, self.fileSize, package);
  if (package.count == 0) {
    SFX_ERR("No payload.\n");
    return 1;
  }

  for (int i = 0; i < package.count; i++) {
    const Payload& payload = package.payloads[i];
    const size_t outputLength = LengthWithoutArchiveExtension(payload.name);
    if (outputLength == 0) {
      if (!WritePayload(payload)) return 1;
      MakeExecutable(payload.name);
    }
    else {
      char output[MAX_FOOTER_SIZE];  // large enough: the name comes from the footer
      memcpy(output, payload.name, outputLength);
      output[outputLength] = '\0';
      FileMemRead archive(payload.data, payload.size);
      if (DecompressArchive(&archive, output) != 0) {
        SFX_ERR("Cannot decompress: %s\n", payload.name);
        return 1;
      }
      MakeExecutable(output);
    }
  }
  return 0;
}

int main(int argc, char** argv) {
  if (argc == 1 || (argc == 2 && strcmp(argv[1], "-x") == 0)) {
    return ExtractAll();
  }
  SFX_ERR("Usage: %s   (extracts the contents)\n", argv[0]);
  return 1;
}

#endif  // SFX_EXTRACT_ONLY

#endif  // SFX


// ============================================================
//  FULL build (paq8sfx, the compressor)
//    paq8sfx -LEVEL input [output]    compress (LEVEL: 1..12)
//    paq8sfx -d archive [output]      decompress
//    paq8sfx -a stubfile scriptfile   assemble a package
// ============================================================
#ifdef FULL

static void PrintUsage() {
  printf("\n"
    "Usage:\n"
    "  " PROGNAME " -LEVEL input [output]     compress a file; LEVEL is 1..12\n"
    "  " PROGNAME " -d archive [output]       decompress an archive\n"
    "  " PROGNAME " -a stubfile scriptfile    assemble a self-extracting package\n"
    "\n"
    "The default output is the input name with ." PROGNAME " appended (compression)\n"
    "or removed (decompression). The output may also be an existing folder.\n"
    "See README.md for the details.\n");
}

enum class Command { None, Compress, Decompress };

// Parses a compression level: 1 or 2 digits and nothing else.
// Returns -1 if text is not a number like that.
static int ParseLevel(const char* text) {
  int level = 0;
  int digits = 0;
  for (; *text >= '0' && *text <= '9'; text++) {
    level = level * 10 + (*text - '0');
    digits++;
  }
  return (*text == '\0' && digits >= 1 && digits <= 2) ? level : -1;
}

static int CompressOrDecompress(int argc, char** argv) {
  Command command = Command::None;
  int level = 0;
  FileName input;
  FileName output;

  // The command line

  for (int i = 1; i < argc; i++) {
    const char* arg = argv[i];
    if (arg[0] == '-') {
      if (arg[1] == '\0') {
        quit("Empty command.");
      }
      if (command != Command::None) {
        quit("Only one command may be specified.");
      }
      if (strcasecmp(arg, "-d") == 0) {
        command = Command::Decompress;
      }
      else if (ParseLevel(arg + 1) >= 0) {
        level = ParseLevel(arg + 1);
        if (level < 1 || level > 12) {
          quit("Compression level must be between 1 and 12.");
        }
        command = Command::Compress;
      }
      else {
        printf("Invalid command: %s", arg);
        quit();
      }
    }
    else if (input.strsize() == 0) {
      input += arg;
      input.replaceSlashes();
    }
    else if (output.strsize() == 0) {
      output += arg;
      output.replaceSlashes();
    }
    else {
      quit("More than two file names specified. Only an input and an output is needed.");
    }
  }

  if (command == Command::None) {
    quit("A command switch is required: -1..-12 to compress, -d to decompress,\n"
         "-a stubfile scriptfile to assemble a self-extracting package.");
  }
  if (input.strsize() == 0) {
    quit(command == Command::Compress ? "An input file is required for compressing."
                                      : "An archive file is required for decompressing.");
  }

  // The input: an existing file. It is split into path and file name.

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
  const FileName inputFile(input.c_str());
  FileName inputPath;
  if (input.lastSlashPos() >= 0) {
    inputPath += input.c_str();
    inputPath.keepPath();
    input.keepFilename();
  }

  // The output: a file name (existing or not), a folder, or nothing.
  // It is split into path and file name; the file name may stay empty.

  FileName outputPath;
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
      if (!outputPath.endsWith("/") && !outputPath.endsWith("\\")) {
        outputPath += GOODSLASH;
      }
      output.resize(0);
      output.pushBack(0);
    }
    else {
      printf("\nThere is a problem with the specified output: %s", output.c_str());
      quit();
    }
  }

  // Verifies that the input is readable, and not too large.
  const uint64_t inputSize = getFileSize(inputFile.c_str());

  // Without an output file name: the input name with the archive extension
  // appended (compression) or removed (decompression).
  if (output.strsize() == 0) {
    output += input.c_str();
    if (command == Command::Compress) {
      output += ARCHIVE_EXTENSION;
    }
    else if (output.endsWith(ARCHIVE_EXTENSION)) {
      output.stripEnd(static_cast<int>(strlen(ARCHIVE_EXTENSION)));
    }
    else {
      printf("\nCan't construct output filename from archive filename.\n"
        "Archive file extension must be: '%s'", ARCHIVE_EXTENSION);
      quit();
    }
  }
  FileName outputFile(outputPath.c_str());
  outputFile += output.c_str();

  // Compress or decompress

  Shared shared;
  FileDisk archive;

  if (command == Command::Compress) {
    printf("\nCreating archive %s ...\n", outputFile.c_str());
    archive.create(outputFile.c_str());
    archive.append(PROGNAME);
    archive.putChar(static_cast<uint8_t>(level));

    shared.init(level);
    PredictorMain predictorMain(&shared);
    Encoder en(&predictorMain, COMPRESS, &archive);

    printf("\nFilename: %s (%" PRIu64 " bytes)\n", inputFile.c_str(), inputSize);
    compressfile(&shared, inputFile.c_str(), inputSize, en);
    en.flush();
    printf("-----------------------\n");
    printf("Total input size     : %" PRIu64 "\n", inputSize);
    printf("Total archive size   : %" PRIu64 "\n", en.size());
    printf("\n");
  }
  else {
    archive.open(inputFile.c_str(), /*mustSucceed=*/true);
    level = ReadArchiveHeader(&archive);
    if (level == 0) {
      printf("\n%s: not a valid %s archive.", inputFile.c_str(), PROGNAME);
      quit();
    }
    printf("\n");

    shared.init(level);
    PredictorMain predictorMain(&shared);
    Encoder en(&predictorMain, DECOMPRESS, &archive);
    decompressFile(&shared, outputFile.c_str(), FMode::FDECOMPRESS, en);
  }

  archive.close();
  return 0;
}

// Assembles a package from a stub file. The stub file may be a bare stub or
// a package; from a package only the stub part is used.
static int AssembleWithStub(const char* stubFile, const char* scriptFile) {
  FileDisk file;
  file.open(stubFile, /*mustSucceed=*/true);
  file.setEnd();
  const uint64_t size = file.curPos();
  file.setpos(0);
  Array<uint8_t> image(size);
  if (size == 0 || file.blockRead(&image[0], size) != size) {
    printf("Cannot read stub: %s\n", stubFile);
    return 1;
  }
  file.close();
  return Assemble(&image[0], size, scriptFile) ? 0 : 1;
}

int main(int argc, char** argv) {
  printf(PROGNAME " v" PROGVERSION " (" PROGYEAR ") - a self-extracting archive creator with the power of paq8px\n");
  if (argc == 1) {
    PrintUsage();
    return 1;
  }
  if (strcmp(argv[1], "-a") == 0) {
    if (argc != 4) quit("Usage: " PROGNAME " -a stubfile scriptfile");
    return AssembleWithStub(argv[2], argv[3]);
  }
  return CompressOrDecompress(argc, argv);
}

#endif  // FULL
