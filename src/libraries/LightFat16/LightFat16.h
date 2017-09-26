#ifndef LIGHT_FAT_16_H
#define LIGHT_FAT_16_H

#include <Arduino.h>
#include <SdCard.h>

#define LF16_FILE_NAME_NUMBER_SIZE 2
#define LF16_FILE_NAME_NUMBER_LIMIT 100
#define LF16_DEFAULT_BASE_FILE_NAME "GPS00"

/* the first 3 characters give the extension */
/* ex : TXT -> 0x54,0x58,0x54 */
#define LF16_FILE_ENTRY_CONSTANTS {0x49,0x47,0x43,0x20,0x00,0x64,0xD0,0x89,0x26,0x49,0x26,0x49,0x00,0x00,0xD0,0x89,0x26,0x49}
#define LF16_FILE_ENTRY_CONSTANTS_SIZE 18

#define LF16_BLOCK_SIZE 512

class lightfat16 {

 public :
  lightfat16();
  int init(int sspin, uint8_t sckDivisor = SPI_CLOCK_DIV2);
  int begin(void);
  int begin(char* fileName, uint8_t fileNameSize); //!! last bytes are used for the incrementing number
  void write(uint8_t inByte);
  void sync();
  

 private:
  SdCard card;
  uint32_t currentBlock;
  unsigned currentPos;
  uint8_t blockBuffer[LF16_BLOCK_SIZE];
  boolean blockWriteEnabled;

  uint8_t blocksPerCluster;
  uint16_t blocksPerFAT;
  uint32_t dataBlock;
  uint32_t fileEntryBlock;
  unsigned fileEntryPos;
  uint32_t fileFATBlock;
  unsigned fileFATPos;
  unsigned fileCluster;
  uint32_t fileDataBlock;
  unsigned fileDataPos;
  uint8_t fileClusterBlockUsage;

  void fileNewBlock();
  void fileNextCluster();
  void blockWriteEnable(boolean reloadBlock = true);
  void blockWriteDisable(boolean writeCurrentChange = true);
  void blockWriteSync();
  uint8_t* blockSet(uint32_t block, unsigned pos, boolean loadBlock = true);
  uint8_t* blockSeek(int32_t byteShift, int32_t blockShift = 0, boolean loadBlock = true);
  void blockGetPosition(uint32_t& block, unsigned& pos);
  
};


#endif
