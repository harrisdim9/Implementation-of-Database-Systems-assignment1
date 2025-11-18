#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bf.h"
#include "hp_file.h"
#include "record.h"

// Just a macro that makes error checking cleaner
#define CALL_BF(call, errorSignal)  \
{                                   \
  BF_ErrorCode code = call;         \
  if (code != BF_OK) {              \
    return errorSignal;             \
  }                                 \
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////// Useful general purpose functions //////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

int HP_isFull(HP_info* hp_info){
  // If numOfRecords is 0 then only the header block has been made, no records can be inserted
  // in the header block so our heap is full. Else I check if last block is full
  return (hp_info->numOfRecords == 0 || hp_info->lastBlockNumOfRecs == hp_info->recsPerBlock);
}

block_info* getBlockHeader(BF_Block* block){
  // Returns a pointer to the header of a block
  void* data = BF_Block_GetData(block);
  data += BF_BLOCK_SIZE - sizeof(block_info);
  return (block_info*) data;
}

block_info* createNewBlock(int fileDesc, BF_Block* block){
  // Creates a new block and returns pointer to the header of the block
  CALL_BF(BF_AllocateBlock(fileDesc, block), NULL);
  block_info* blockHeader = getBlockHeader(block);
  blockHeader->numOfRecords = 0;
  return blockHeader;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////// Functions for the implementation of the heap //////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

int HP_CreateFile(char *fileName){
  // Functions that crates a heap file
  HP_info newHeader;                                             // Creates the data I will write in header block
  newHeader.numOfRecords = 0;
  newHeader.lastBlockNumOfRecs = 0;
  // How many records can fit in a block
  newHeader.recsPerBlock = (BF_BLOCK_SIZE - sizeof(block_info)) / sizeof(Record);

  CALL_BF(BF_CreateFile(fileName), -1);                          // Creates the file

  int fileDesc;
  CALL_BF(BF_OpenFile(fileName, &fileDesc), -1);                 // Opens the file that was just created

  BF_Block* headerBlock;                                         // Creates a new block where header will be stored
  BF_Block_Init(&headerBlock);
  CALL_BF(BF_AllocateBlock(fileDesc, headerBlock), -1);
  
  void* data = BF_Block_GetData(headerBlock);
  memcpy(data, &newHeader, sizeof(HP_info));                     // Stores the data of the header
  
  BF_Block_SetDirty(headerBlock);                                // Sets block as dirty, unpins it, frees memory of block entity
  CALL_BF(BF_UnpinBlock(headerBlock), -1);
  BF_Block_Destroy(&headerBlock);

  BF_CloseFile(fileDesc);                                        // Closes the file

  return 0;                                                      // Signals that everything went well
}

HP_info* HP_OpenFile(char *fileName, int *fileDesc){
  // Function that opens a heap file
  HP_info* hpInfoPtr;
  
  CALL_BF(BF_OpenFile(fileName, fileDesc), NULL);                // Opens the file

  BF_Block* headerBlock;                                         // I get it's header block, (it gets automatically pinned too)
  BF_Block_Init(&headerBlock);
  CALL_BF(BF_GetBlock(*fileDesc, 0, headerBlock), NULL);

  void* data = BF_Block_GetData(headerBlock);                    // I get the address of where the data of the block is stored in memory
  hpInfoPtr = data;                                              // Stores it in hpInfoPtr, done so compiler doesnt warn, we want to return type HPT_info* not void*

  BF_Block_Destroy(&headerBlock);                                // Destroys the block data structure now that we are done

  return hpInfoPtr;                                              // Returns the adress of the header block's data
}

int HP_CloseFile(int fileDesc, HP_info* header_info){
  // Function that closes a heap file
  BF_Block* headerBlock;                                         // Initializes a block's vesel
  BF_Block_Init(&headerBlock);

  BF_GetBlock(fileDesc, 0, headerBlock);                         // Gets header block from memory

  BF_Block_SetDirty(headerBlock);                                // Since the file closes we no longer will need this header block in memory
  CALL_BF(BF_UnpinBlock(headerBlock), -1);                       // We set it dirty in case we made any changes to it, and unpin it so that
  BF_Block_Destroy(&headerBlock);                                // it is transferred to the disk later at some point

  CALL_BF(BF_CloseFile(fileDesc), -1);                           // Closes the file

  header_info = NULL;                                            // The header info no longer points anywhere since block got unpinned

  return 0;                                                      // Signals that everything went well
}

int HP_InsertEntry(int fileDesc, HP_info* hp_info, Record record){
  // Function that inserts a record in a heap file
  int numOfBlocks;
  block_info* blockHeader;
  void* data;
  BF_Block *block;                                               // initializes a vesel for the entry block
  BF_Block_Init(&block);

  if(HP_isFull(hp_info)){
    blockHeader = createNewBlock(fileDesc, block);
    if(blockHeader == NULL) return -1;                           // Null is returned means creation has failed
    data = BF_Block_GetData(block);
    memcpy(data, &record, sizeof(Record));
    blockHeader->numOfRecords += 1;
    hp_info->lastBlockNumOfRecs = 1;
  } else{                                                        // Else we insert in current last block
    CALL_BF(BF_GetBlockCounter(fileDesc, &numOfBlocks), -1);
    CALL_BF(BF_GetBlock(fileDesc, numOfBlocks - 1, block), -1);
    blockHeader = getBlockHeader(block);
    void* data = BF_Block_GetData(block);
    data += sizeof(Record) * hp_info->lastBlockNumOfRecs;
    memcpy(data, &record, sizeof(Record));
    blockHeader->numOfRecords += 1;
    hp_info->lastBlockNumOfRecs += 1;
  }

  hp_info->numOfRecords += 1;                                    // hp_info has been updated

  BF_Block_SetDirty(block);                                      // Block is set dirty, unpinned and vesel is destroyed
  CALL_BF(BF_UnpinBlock(block), -1);
  BF_Block_Destroy(&block);

  BF_GetBlockCounter(fileDesc, &numOfBlocks);                    // Returns number of blocks
  return numOfBlocks;
}

int HP_GetAllEntries(int fileDesc, HP_info* hp_info, int id){
  // Function that prints all records that got key equal to value
  void* data;
  int numOfBlocks; 
  CALL_BF(BF_GetBlockCounter(fileDesc, &numOfBlocks), -1);

  BF_Block* block;                                               // initializes data structure to store blocks in
  BF_Block_Init(&block);

  for(int i = 1; i < numOfBlocks; i++){                          // Loops through all the blocks

    CALL_BF(BF_GetBlock(fileDesc, i, block), -1);                // Gets the block
    block_info* blockHeader = getBlockHeader(block);             // Gets it's header
    int recsInBlock = blockHeader->numOfRecords;                 // Gets number of records in block
    data = BF_Block_GetData(block);                              // Gets pointer to it's data
    Record* rec = data;                                          // Cool trick allows us to use syntax rec[i] instead of *(data + i * sizeof(Record))

    for(int j = 0; j < recsInBlock; j++){                        // Loops through all the records
      if(rec[j].id == id){                                       // Checks if record has id equal to value
        printRecord(rec[j]);                                     // If true prints it
      }
    }

    CALL_BF(BF_UnpinBlock(block), -1);                           // We dont forget to unpin blocks after we no longer need them
  
  }

  BF_Block_Destroy(&block);                                      // And destroy the block data structure now that we are done

  return numOfBlocks;                                            // And GG we just finished the function and return numOfBlocks
}

