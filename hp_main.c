#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "bf.h"
#include "hp_file.h"

#define RECORDS_NUM 1000 // you can change it if you want
#define FILE_NAME "data.db"

#define CALL_OR_DIE(call)     \
  {                           \
    BF_ErrorCode code = call; \
    if (code != BF_OK) {      \
      BF_PrintError(code);    \
      exit(code);             \
    }                         \
  }

int testRequirements(){
  if(sizeof(HP_info) > BF_BLOCK_SIZE || sizeof(Record) > BF_BLOCK_SIZE){
    return -1;
  }
  return 0;
}

int main() {
  BF_Init(LRU);

  int errSignal, fileDescriptor;
  errSignal = testRequirements();
  if(errSignal == -1){
    printf("Block size is too small for the heap we implemented to work. \nSince we do not support splitting records in 2 blocks program cannot run.\n");
  }

  errSignal = HP_CreateFile(FILE_NAME);

  if(errSignal == -1){
    printf("Cannot create file. \nFile with name already exists in disk. \nChoose a different name and try again.\n");
    return 1;
  }

  HP_info* info = HP_OpenFile(FILE_NAME, &fileDescriptor);

  Record record;
  srand(12569874);
  int r;

  printf("Insert Entries\n");
  for (int id = 0; id < RECORDS_NUM; ++id) {
    record = randomRecord();
    HP_InsertEntry(fileDescriptor,  info, record);
  }

  printf("RUN PrintAllEntries\n");
  int id = rand() % RECORDS_NUM;
  printf("\nSearching for: %d",id);
  HP_GetAllEntries(fileDescriptor, info, id);

  printf("\nSearches for people with ids that end with number 00:\n");
  for(int id = 100; id < RECORDS_NUM; id += 100){
    HP_GetAllEntries(fileDescriptor, info, id);
  }

  HP_CloseFile(fileDescriptor, info);
  BF_Close();

  return 0;
}
