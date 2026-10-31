#include "Filters.h"
#include "exe.h"
#include "../Array.h"
#include "../Block.h"
#include "../BlockType.h"
#include "../String.h"
#include "../file/File.h"
#include "../file/FileDisk.h"
#include "../file/FileTmp.h"
#include "../Utils.h"

//////////////////// Decompress ////////////////////////////

// Decodes a block that was transformed before compression
static uint64_t decodeFunc(BlockType type, Encoder* en, File* tmp, uint64_t len, int info, File* out, FMode mode, uint64_t* diffFound) {
  if (type == BLOCKTYPE_EXE) {
    ExeFilter f;
    ExeFilter_init(&f);
    ExeFilter_setBegin(&f, info);
    ExeFilter_setEncoder(&f, en);
    return ExeFilter_decode(&f, tmp, out, mode, len, diffFound);
  }
  else {
    assert(false);
  }
  return 0;
}

// Decompresses (or compares) blockSize bytes, block by block
static uint64_t decompressRecursive(File* out, uint64_t blockSize, Encoder* en, FMode mode) {
  uint64_t i = 0;
  uint64_t diffFound = 0;
  while (i < blockSize) {

    uint64_t len = Block_DecodeBlockHeader(en);
    BlockType type = en->predictorMain->shared->State.blockType;
    int info = en->predictorMain->shared->State.blockInfo;
    if (hasTransform(type, info)) {
      len = decodeFunc(type, en, NULL, len, info, out, mode, &diffFound);
    }
    else {
      for (uint64_t j = 0; j < len; ++j) {
        if ((j & 0xfff) == 0) {
          Encoder_printStatus(en);
        }
        if (mode == FDECOMPRESS) {
          File_putChar(out, Encoder_decompressByte(en, en->predictorMain));
        }
        else if (mode == FCOMPARE) {
          if (Encoder_decompressByte(en, en->predictorMain) != File_getChar(out) && (diffFound == 0)) {
            mode = FDISCARD;
            diffFound = i + j + 1;
          }
        }
        else {
          Encoder_decompressByte(en, en->predictorMain);
        }
      }
    }
    i += len;
  }
  return diffFound;
}

void decompressFile(const Shared* const shared, const char* filename, FMode fMode, Encoder* en) {
  (void)shared;
  assert(Encoder_getMode(en) == DECOMPRESS);
  assert(filename && filename[0]);

  Encoder_initContextForBlockModel(en, BLOCKTYPE_DEFAULT, 0);
  uint64_t fileSize = Block_DecodeBlockSize(en);

  FileDisk f;
  FileDisk_init(&f);
  if (fMode == FCOMPARE) {
    File_open(&f.base, filename, true);
    printf("Comparing");
  }
  else { //mode==FDECOMPRESS;
    File_create(&f.base, filename);
    printf("Extracting");
  }
  printf(" %s %" PRIu64 " bytes -> ", filename, fileSize);

  // Decompress/Compare
  uint64_t r = decompressRecursive(&f.base, fileSize, en, fMode);
  if (fMode == FCOMPARE && (r == 0) && File_getChar(&f.base) != EOF) {
    printf("file is longer\n");
  }
  else if (fMode == FCOMPARE && (r != 0)) {
    printf("differ at %" PRIu64 "\n", r - 1);
  }
  else if (fMode == FCOMPARE) {
    printf("identical\n");
  }
  else {
    printf("done   \n");
  }
  File_close(&f.base);
}

#ifdef FULL

//////////////////// Detect ////////////////////////////////

typedef struct DetectionInfo
{
  uint64_t DataStart;
  uint64_t DataLength;
  BlockType Type;
  int DataInfo;
} DetectionInfo;

// Finds the next x86/x64 block in the next blockSize bytes of the file.
// Returns its position and length, or a block of length 0 at the end if there is none.
static DetectionInfo detect(File *in, uint64_t blockSize) {

  DetectionInfo detectionInfo;
  memset(&detectionInfo, 0, sizeof(detectionInfo));

  // TODO: Large file support
  const uint64_t n = blockSize;

  // last 8 bytes
  uint32_t buf1 = 0;
  uint32_t buf0 = 0;

  const uint64_t start = File_curPos(in); // start of the current block

  // For EXE detection
  uint64_t absPos[256]; // CALL/JMP abs. address. low byte -> last offset
  uint64_t relPos[256]; // CALL/JMP relative address. low byte -> last offset
  memset(absPos, 0, sizeof(absPos));
  memset(relPos, 0, sizeof(relPos));
  int e8e9count = 0; // number of consecutive CALL/JMPs
  uint64_t e8e9pos = 0; // offset of first CALL or JMP instruction
  uint64_t e8e9last = 0; // offset of most recent CALL or JMP

  for (uint64_t i = 0; i < n; ++i) {
    int c = File_getChar(in);
    if (c == EOF) {
      quit("detect(): Unexpected end of file");
    }

    buf1 = buf1 << 8 | buf0 >> 24;
    buf0 = buf0 << 8 | c;

    // Detect x86/64 if the low order byte (little-endian) XX is more
    // recently seen (and within 4K) if a relative to absolute address
    // conversion is done in the context CALL/JMP (E8/E9) XX xx xx 00/FF
    // 4 times in a row.  Detect end of EXE at the last
    // place this happens when it does not happen for 64KB.

    if (((buf1 & 0xfe) == 0xe8 || (buf1 & 0xfff0) == 0x0f80) && ((buf0 + 1) & 0xfe) == 0) {
      uint64_t r = buf0 >> 24; // relative address low 8 bits
      uint64_t a = ((buf0 >> 24) + i) & 0xff; // absolute address low 8 bits
      uint64_t rDist = i - relPos[r];
      uint64_t aDist = i - absPos[a];
      if (aDist < rDist && aDist < 0x800 && absPos[a] > 5) {
        e8e9last = i;
        ++e8e9count;
        if (e8e9pos == 0 || e8e9pos > absPos[a]) {
          e8e9pos = absPos[a];
        }
      }
      else {
        e8e9count = 0;
      }
      if (detectionInfo.Type == BLOCKTYPE_DEFAULT && e8e9count >= 4 && e8e9pos > 5) {
        detectionInfo.Type = BLOCKTYPE_EXE;
        detectionInfo.DataStart = start + e8e9pos - 5;
      }
      absPos[a] = i;
      relPos[r] = i;
    }
    if (i + 1 == n || i - e8e9last > 0x4000) {
      // TODO: Large file support
      if (detectionInfo.Type == BLOCKTYPE_EXE) {
        detectionInfo.DataInfo = (int)(detectionInfo.DataStart);
        detectionInfo.DataLength = start + e8e9last - detectionInfo.DataStart;
        return detectionInfo;
      }
      e8e9pos = 0;
      e8e9count = 0;
    }
  }

  if (detectionInfo.Type != BLOCKTYPE_DEFAULT)
    quit("detect(): detection didn't finish properly.");

  //nothing detected
  detectionInfo.Type = BLOCKTYPE_DEFAULT;
  detectionInfo.DataStart = start + n;
  detectionInfo.DataLength = 0;

  return detectionInfo;
}

//////////////////// Compress //////////////////////////////

// Compresses a block as it is
static void directEncodeBlock(BlockType type, File *in, uint64_t len, Encoder* en, int info) {
  // TODO: Large file support
  Block_EncodeBlockHeader(en, type, len, info);
  fprintf(stderr, "Compressing... ");
  for( uint64_t j = 0; j < len; ++j ) {
    if((j & 0xfff) == 0 ) {
      Encoder_printStatusRange(en, j, len);
    }
    Encoder_compressByte(en, en->predictorMain, (uint8_t)File_getChar(in));
  }
  fprintf(stderr, "\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b");
}

// Transforms a block to a better compressible form, into tmp
static void encodeFunc(BlockType type, File *in, File *tmp, uint64_t len, int info, int* hdrsize) {
  if( type == BLOCKTYPE_EXE ) {
    ExeFilter f;
    ExeFilter_init(&f);
    ExeFilter_setBegin(&f, info);
    ExeFilter_encode(&f, in, tmp, len, info, hdrsize);
  } else {
    assert(false);
  }
}

// Compresses a block. A block of a type that has a transform is transformed
// first. The transform is then tested: if decoding does not give back the
// original data, the block is compressed without the transform.
static void transformEncodeBlock(BlockType type, File *in, uint64_t len, Encoder* en, int info, uint64_t begin) {
  if( hasTransform(type, info)) {
    FileTmp tmp;
    FileTmp_init(&tmp);
    int headerSize = 0;
    encodeFunc(type, in, &tmp.base, len, info, &headerSize);
    const uint64_t tmpSize = File_curPos(&tmp.base);

    // Test: decode the transformed data and compare it with the input
    uint64_t diffFound = 0;
    File_setpos(&tmp.base, 0);
    Encoder_setFile(en, &tmp.base);
    File_setpos(in, begin);
    decodeFunc(type, en, &tmp.base, tmpSize, info, in, FCOMPARE, &diffFound);

    // Test fails, compress without transform
    if( diffFound > 0 || File_getChar(&tmp.base) != EOF) {
      printf("Transform fails at %" PRIu64 ", skipping...\n", diffFound - 1);
      File_setpos(in, begin);
      directEncodeBlock(BLOCKTYPE_DEFAULT, in, len, en, -1);
    } else {
      File_setpos(&tmp.base, 0);
      directEncodeBlock(type, &tmp.base, tmpSize, en, info);
    }
    File_close(&tmp.base);
  } else {
    directEncodeBlock(type, in, len, en, info);
  }
}

static void printBlock(const uint64_t begin, const uint64_t len, const BlockType type, const int blNum) {
  static const char* typeNames[] = { "default", "x86/64" };

  String blockName;
  String_init(&blockName, "");
  String_appendInt(&blockName, (uint64_t)blNum);
  printf(" %-11s | %-16s |%10" PRIu64 " bytes [%" PRIu64 " - %" PRIu64 "]\n", String_c_str(&blockName), typeNames[(int)type], len, begin, (begin + len) - 1);
  String_destroy(&blockName);
}

static void compressBlock(File* in, const uint64_t begin, const uint64_t len, int* blNum, BlockType type, int blockInfo, Encoder* en, float* p1, float* p2, const float pscale) {
  *p2 = *p1 + pscale * len;
  Encoder_setStatusRange(en, *p1, *p2);

  printBlock(begin, len, type, *blNum);
  transformEncodeBlock(type, in, len, en, blockInfo, begin);
  (*blNum)++;

  *p1 = *p2;
}

// Splits the next bytesToProcess bytes of the file into blocks by type and
// compresses them one by one. p1..p2 is the range of the progress display.
static void compressRecursive(File *in, uint64_t bytesToProcess, Encoder* en, float p1, float p2) {

  uint64_t begin = File_curPos(in);

  float pscale = bytesToProcess != 0 ? (p2 - p1) / bytesToProcess : 0;

  int blNum = 0;
  while(bytesToProcess > 0 ) {

    //detect a block
    DetectionInfo detectionInfo = detect(in, bytesToProcess); // Special blocktypes
    File_setpos(in, begin);

    //everything in front of the detected block is a default block
    if (detectionInfo.DataStart != begin) {
      const uint64_t len = detectionInfo.DataStart - begin;
      compressBlock(in, begin, len, &blNum, BLOCKTYPE_DEFAULT, 0, en, &p1, &p2, pscale);
      begin += len;
      bytesToProcess -= len;
    }

    //the detected block
    if (detectionInfo.DataLength != 0) {
      compressBlock(in, detectionInfo.DataStart, detectionInfo.DataLength, &blNum, detectionInfo.Type, detectionInfo.DataInfo, en, &p1, &p2, pscale);
      begin += detectionInfo.DataLength;
      bytesToProcess -= detectionInfo.DataLength;
    }
  }
}

void compressfile(const Shared* const shared, const char *filename, uint64_t fileSize, Encoder* en) {
  (void)shared;
  assert(Encoder_getMode(en) == COMPRESS);
  assert(filename && filename[0]);

  Encoder_initContextForBlockModel(en, BLOCKTYPE_DEFAULT, 0);
  Block_EncodeBlockSize(en, fileSize);

  FileDisk in;
  FileDisk_init(&in);
  File_open(&in.base, filename, true);
  printf("Block segmentation:\n");
  compressRecursive(&in.base, fileSize, en, 0.0F, 1.0F);
  File_close(&in.base);
}

#endif // FULL
