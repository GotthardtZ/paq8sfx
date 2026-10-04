#pragma once

#include "exe.hpp"
#include "../Array.hpp"
#include "../BlockType.hpp"
#include "../Encoder.hpp"
#include "../file/File.hpp"
#include "../file/FileDisk.hpp"
#include "../file/FileTmp.hpp"
#include "../Utils.hpp"
#include "Filter.hpp"
#include <cctype>
#include <cstdint>
#include <cstring>

static uint64_t decodeFunc(BlockType type, Encoder& en, File* tmp, uint64_t len, int info, File* out, FMode mode, uint64_t& diffFound) {
  if (type == BlockType::EXE) {
    auto f = ExeFilter();
    f.setBegin(info);
    f.setEncoder(en);
    return f.decode(tmp, out, mode, len, diffFound);
  }
  else {
    assert(false);
  }
  return 0;
}

static uint64_t decompressRecursive(File* out, uint64_t blockSize, Encoder& en, FMode mode) {
  uint64_t i = 0;
  uint64_t diffFound = 0;
  while (i < blockSize) {

    uint64_t len = Block::DecodeBlockHeader(&en);
    BlockType type = en.predictorMain->shared->State.blockType;
    int info = en.predictorMain->shared->State.blockInfo;
    if (hasTransform(type, info)) {
      len = decodeFunc(type, en, nullptr, len, info, out, mode, diffFound);
    }
    else {
      for (uint64_t j = 0; j < len; ++j) {
        if ((j & 0xfff) == 0) {
          en.printStatus();
        }
        if (mode == FMode::FDECOMPRESS) {
          out->putChar(en.decompressByte(en.predictorMain));
        }
        else if (mode == FMode::FCOMPARE) {
          if (en.decompressByte(en.predictorMain) != out->getchar() && (diffFound == 0)) {
            mode = FMode::FDISCARD;
            diffFound = i + j + 1;
          }
        }
        else {
          en.decompressByte(en.predictorMain);
        }
      }
    }
    i += len;
  }
  return diffFound;
}

// Decompress or compare a file
static void decompressFile(const Shared* const shared, const char* filename, FMode fMode, Encoder& en) {
  assert(en.getMode() == DECOMPRESS);
  assert(filename && filename[0]);

  en.initContextForBlockModel(BlockType::DEFAULT, 0);
  uint64_t fileSize = Block::DecodeBlockSize(&en);

  FileDisk f;
  if (fMode == FMode::FCOMPARE) {
    f.open(filename, true);
    printf("Comparing");
  }
  else { //mode==FDECOMPRESS;
    f.create(filename);
    printf("Extracting");
  }
  printf(" %s %" PRIu64 " bytes -> ", filename, fileSize);

  // Decompress/Compare
  uint64_t r = decompressRecursive(&f, fileSize, en, fMode);
  if (fMode == FMode::FCOMPARE && (r == 0) && f.getchar() != EOF) {
    printf("file is longer\n");
  }
  else if (fMode == FMode::FCOMPARE && (r != 0)) {
    printf("differ at %" PRIu64 "\n", r - 1);
  }
  else if (fMode == FMode::FCOMPARE) {
    printf("identical\n");
  }
  else {
    printf("done   \n");
  }
  f.close();
}

#ifdef FULL

struct DetectionInfo
{
  uint64_t HeaderStart{};
  uint64_t HeaderLength{};
  uint64_t DataStart{};
  uint64_t DataLength{};
  BlockType Type{};
  int DataInfo{};
};

  struct TextDetectionInfo
  {
    uint64_t DataStart{};
    uint64_t DataLength{};
    BlockType Type{}; // DEFAULT / TEXT / TEXT_EOL
  };

  // Detect text blocks (TEXT/TEXT_EOL) inside a DEFAULT block
  static TextDetectionInfo detectText(File* in, uint64_t blockStart, uint64_t blockSize) {
    TextDetectionInfo detectionInfo;

    //no text found at all, or it is too small
    //DEFAULT
    detectionInfo.Type = BlockType::DEFAULT;
    detectionInfo.DataStart = blockStart;
    detectionInfo.DataLength = blockSize;
    return detectionInfo;
  }

  
// Detect blocks
static DetectionInfo detect(File *in, uint64_t blockSize) {

  DetectionInfo detectionInfo;

  int textParserState = 0;
  uint64_t blockHash = 0;

  // TODO: Large file support
  const uint64_t n = blockSize;

  // last 16 bytes
  uint32_t buf3 = 0;
  uint32_t buf2 = 0;
  uint32_t buf1 = 0;
  uint32_t buf0 = 0;
  
  uint64_t start = 0;

  start = in->curPos(); // start of the current block
  
  // For EXE detection
  Array<uint64_t> absPos(256); // CALL/JMP abs. address. low byte -> last offset
  Array<uint64_t> relPos(256); // CALL/JMP relative address. low byte -> last offset
  int e8e9count = 0; // number of consecutive CALL/JMPs
  uint64_t e8e9pos = 0; // offset of first CALL or JMP instruction
  uint64_t e8e9last = 0; // offset of most recent CALL or JMP

  for (uint64_t i = 0; i < n; ++i) {
    int c = in->getchar();
    if (c == EOF) {
      quit("detect(): Unexpected end of file");
    }

    blockHash = hash(blockHash, c);

    buf3 = buf3 << 8 | buf2 >> 24;
    buf2 = buf2 << 8 | buf1 >> 24;
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
      if (detectionInfo.Type == BlockType::DEFAULT && e8e9count >= 4 && e8e9pos > 5) {
        detectionInfo.Type = BlockType::EXE;
        detectionInfo.DataStart = start + e8e9pos - 5;
      }
      absPos[a] = i;
      relPos[r] = i;
    }
    if (i + 1 == n || i - e8e9last > 0x4000) {
      // TODO: Large file support
      if (detectionInfo.Type == BlockType::EXE) {
        detectionInfo.DataInfo = static_cast<int>(detectionInfo.DataStart);
        detectionInfo.DataLength = start + e8e9last - detectionInfo.DataStart;
        return detectionInfo;
      }
      e8e9pos = 0;
      e8e9count = 0;
    }
  }
  
  if (detectionInfo.Type != BlockType::DEFAULT)
    quit("detect(): detection didn't finish properly.");

  //nothing detected
  detectionInfo.Type = BlockType::DEFAULT;
  detectionInfo.DataStart = start + n;
  detectionInfo.DataLength = 0;

  return detectionInfo;
}

//////////////////// Compress, Decompress ////////////////////////////

static void directEncodeBlock(BlockType type, File *in, uint64_t len, Encoder &en, int info) {
  // TODO: Large file support
  Block::EncodeBlockHeader(&en, type, len, info);
  fprintf(stderr, "Compressing... ");
  for( uint64_t j = 0; j < len; ++j ) {
    if((j & 0xfff) == 0 ) {
      en.printStatus(j, len);
    }
    en.compressByte(en.predictorMain, in->getchar());
  }
  fprintf(stderr, "\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b");
}

static uint64_t encodeFunc(BlockType type, File *in, File *tmp, uint64_t len, int info, int &hdrsize) {
  if( type == BlockType::EXE ) {
    auto f = ExeFilter();
    f.setBegin(info); 
    f.encode(in, tmp, len, info, hdrsize);
  } else {
    assert(false);
  }
  return 0;
}

static void
transformEncodeBlock(BlockType type, File *in, uint64_t len, Encoder &en, int info, String &blstr, float p1, float p2, uint64_t begin) {
  if( hasTransform(type, info)) {
    FileTmp tmp;
    int headerSize = 0;
    uint64_t diffFound = encodeFunc(type, in, &tmp, len, info, headerSize);
    const uint64_t tmpSize = tmp.curPos();
    tmp.setpos(tmpSize); //switch to read mode
    if( diffFound == 0 ) {
      tmp.setpos(0);
      en.setFile(&tmp);
      in->setpos(begin);
      decodeFunc(type, en, &tmp, tmpSize, info, in, FMode::FCOMPARE, diffFound);
    }
    // Test fails, compress without transform
    if( diffFound > 0 || tmp.getchar() != EOF) {
      printf("Transform fails at %" PRIu64 ", skipping...\n", diffFound - 1);
      in->setpos(begin);
      directEncodeBlock(BlockType::DEFAULT, in, len, en, -1);
    } else {
      tmp.setpos(0);
      directEncodeBlock(type, &tmp, tmpSize, en, info);
    }
    tmp.close();
  } else {
    directEncodeBlock(type, in, len, en, info);
  }
}

static void composeSubBlockStringToPrint(String& blstr, String& blstrSub, int blNum) {
  //Compose block enumeration string
  blstrSub += blstr.c_str();
  if (blstrSub.strsize() != 0) {
    blstrSub += "-";
  }
  blstrSub += uint64_t(blNum);
}

static void printBlock(const uint64_t begin, const uint64_t len, const BlockType type, const int blockInfo, String& blstrSub) {
  static const char* typeNames[30] = { "default", "x86/64"};

  const char* typeName = typeNames[(int)type];
  printf(" %-11s | %-16s |%10" PRIu64 " bytes [%" PRIu64 " - %" PRIu64 "]", blstrSub.c_str(), typeName, len, begin, (begin + len) - 1);
  printf("\n");
}

static void compressBlock(File* in, const uint64_t begin, const uint64_t len, int &blNum, BlockType type, int blockInfo, Encoder& en, String& blstr, float &p1, float &p2, const float pscale) {
  p2 = p1 + pscale * len;
  en.setStatusRange(p1, p2);

  String blstrSub;
  composeSubBlockStringToPrint(blstr, blstrSub, blNum);
  printBlock(begin, len, type, blockInfo, blstrSub);
  transformEncodeBlock(type, in, len, en, blockInfo, blstrSub, p1, p2, begin);
  blNum++;

  p1 = p2;
}

static void compressRecursive(File *in, uint64_t bytesToProcess, Encoder &en, String &blstr, float p1, float p2) {

  uint64_t begin = in->curPos();

  float pscale = bytesToProcess != 0 ? (p2 - p1) / bytesToProcess : 0;

  int blNum = 0;
  while(bytesToProcess > 0 ) {

    //detect a block 
    DetectionInfo detectionInfo = detect(in, bytesToProcess); // Special blocktypes
    in->setpos(begin);

    uint64_t blockStart = detectionInfo.HeaderLength != 0 ? detectionInfo.HeaderStart : detectionInfo.DataStart;
    while(blockStart != begin) {
      TextDetectionInfo textDetectionInfo = detectText(in, begin, blockStart - begin); // DEFAULT / TEXT / TEXT_EOL
      compressBlock(in, textDetectionInfo.DataStart, textDetectionInfo.DataLength, /*ref: */ blNum, textDetectionInfo.Type, 0, en, /*in: */ blstr, /*ref: */ p1, /*ref: */ p2, pscale);
      begin += textDetectionInfo.DataLength;
      bytesToProcess -= textDetectionInfo.DataLength;
    }

    if (begin != detectionInfo.DataStart)
      quit("Internal error in compressRecursive");
    
    if (detectionInfo.DataLength != 0) {
      compressBlock(in, detectionInfo.DataStart, detectionInfo.DataLength, /*ref: */ blNum, detectionInfo.Type, detectionInfo.DataInfo, en, /*in: */ blstr, /*ref: */ p1, /*ref: */ p2, pscale);
      begin += detectionInfo.DataLength;
      bytesToProcess -= detectionInfo.DataLength;
    }
  }
}

// Compress a file. Split fileSize bytes into blocks by type.
// For each block, output
// <type> <size> and call encode_X to convert to type X.
// Test transform and compress.
static void compressfile(const Shared* const shared, const char *filename, uint64_t fileSize, Encoder &en) {
  assert(en.getMode() == COMPRESS);
  assert(filename && filename[0]);

  uint64_t start = en.size();
  en.initContextForBlockModel(BlockType::DEFAULT, 0);
  Block::EncodeBlockSize(&en, fileSize);

  FileDisk in;
  in.open(filename, true);
  printf("Block segmentation:\n");
  String blstr;
  compressRecursive(&in, fileSize, en, blstr, 0.0F, 1.0F);
  in.close();
}

#endif // FULL

