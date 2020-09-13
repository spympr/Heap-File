#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bf.h"
#include "heap_file.h"


#define CALL_BF(call)       \
{                           \
  BF_ErrorCode code = call; \
  if (code != BF_OK) {      \
    BF_PrintError(code);    \
    return HP_ERROR;        \
  }                         \
}
                                                     
HP_ErrorCode HP_Init() {
  return HP_OK;
}

HP_ErrorCode HP_CreateFile(const char *filename) {
  //Create a BF_File...
  CALL_BF(BF_CreateFile(filename));

  //Open the BF_File...
  int fd;
  CALL_BF(BF_OpenFile(filename,&fd));
  
  //KEY_OF_HEAP_FILE will be written to the first block so as to make it heap file
  BF_Block* block;
  BF_Block_Init(&block);
  CALL_BF(BF_AllocateBlock(fd,block)); //Block has pinned.
  
  char* data = BF_Block_GetData(block);
  int num=1024;
  memcpy(data,&num,sizeof(int));
  BF_Block_SetDirty(block);
  CALL_BF(BF_UnpinBlock(block)); //Block has unpinned.

  //Create 2nd block and write inside it number of records.
  CALL_BF(BF_AllocateBlock(fd,block)); //Block has pinned.
  
  data = BF_Block_GetData(block);
  int num_of_records = 0;
  memcpy(data,&num_of_records,sizeof(int));
  BF_Block_SetDirty(block); 
  CALL_BF(BF_UnpinBlock(block)); //Block has unpinned.

  //Closing the BF_File...
  CALL_BF(BF_CloseFile(fd));
  
  //Destroying the block that we used.
  BF_Block_Destroy(&block);
  
  return HP_OK;
}

HP_ErrorCode HP_OpenFile(const char *fileName, int *fileDesc){
  //Opening the BF_File...
  CALL_BF(BF_OpenFile(fileName,fileDesc));
  
  //Check if it is heap file
  BF_Block* block;
  BF_Block_Init(&block);
  CALL_BF(BF_GetBlock(*fileDesc,0,block)); //Block pinned.
  char* data = BF_Block_GetData(block);
  int number;
  memcpy(&number,data,sizeof(int));  
  if(number!=1024)
    return HP_ERROR;
  
  CALL_BF(BF_UnpinBlock(block)); //Unpinned.
  BF_Block_Destroy(&block);
  return HP_OK;
}

  HP_ErrorCode HP_CloseFile(int fileDesc) {
    //Closing the BF_File...
    CALL_BF(BF_CloseFile(fileDesc));
    
    return HP_OK;
  }

HP_ErrorCode HP_InsertEntry(int fileDesc, Record record) {

//Get the last block of heap file.
int blocks_num;
CALL_BF(BF_GetBlockCounter(fileDesc,&blocks_num));
BF_Block* block;
BF_Block_Init(&block);
CALL_BF(BF_GetBlock(fileDesc,blocks_num-1,block)); //Block has pinned.

//How many records are in the block?
int num_of_records;
char* data = BF_Block_GetData(block);
memcpy(&num_of_records,data,sizeof(int));
BF_Block_SetDirty(block);

//If we have enough space for a new record into the block then put it.
int recordbytes = BF_BLOCK_SIZE - num_of_records*sizeof(record) - sizeof(int);
if(recordbytes >= sizeof(record))
{
  memcpy(data+(BF_BLOCK_SIZE-recordbytes),&record,sizeof(record));
  num_of_records++;
  memcpy(data,&num_of_records,sizeof(int));
  CALL_BF(BF_UnpinBlock(block)); //Block has unpinned.
}
//If we don't have enough space for a new record into the block then we need a new block!
else
{
  CALL_BF(BF_UnpinBlock(block)); //Block has unpinned.
  CALL_BF(BF_AllocateBlock(fileDesc,block)); //New block has just pinned.

  char* data = BF_Block_GetData(block);
  num_of_records=0;
  memcpy(data+sizeof(int),&record,sizeof(record));
  num_of_records++;
  memcpy(data,&num_of_records,sizeof(int));
  BF_Block_SetDirty(block);
  CALL_BF(BF_UnpinBlock(block)); //New block has just unpinned.
}

BF_Block_Destroy(&block);
return HP_OK;
}

HP_ErrorCode HP_PrintAllEntries(int fileDesc,char *fieldName,void *value) {

//Useful declarations for both situations.
int blocks_num;
CALL_BF(BF_GetBlockCounter(fileDesc,&blocks_num));  
BF_Block* block;
BF_Block_Init(&block);
char* data;
int num_of_records,i,j;
Record temp;
  
for(i=1;i<(blocks_num);i++)
{
  CALL_BF(BF_GetBlock(fileDesc,i,block));
  data = BF_Block_GetData(block); //Block has just pinned.
  memcpy(&num_of_records,data,sizeof(int));
  data+=sizeof(int);
    
  for(j=0;j<num_of_records;j++)
  {
    memcpy(&temp,data+j*sizeof(Record),sizeof(Record));
      
    if(value==NULL) //Print all block's data.
      printf("|%d|%s|%s|%s| \n", temp.id, temp.name, temp.surname, temp.city);
    else //Print block's data which they have this specific value.
    {
      if(strcmp(fieldName,"name")==0)
      {
        if(strcmp(temp.name,value)==0)
          printf("|%d|%s|%s|%s| \n", temp.id, temp.name, temp.surname, temp.city);
      }
      else if(strcmp(fieldName,"surname")==0)
      {
        if(strcmp(temp.surname,value)==0)
          printf("|%d|%s|%s|%s| \n", temp.id, temp.name, temp.surname, temp.city); 
      }
      else if(strcmp(fieldName,"city")==0)
      {
        if(strcmp(temp.city,value)==0)
          printf("|%d|%s|%s|%s| \n", temp.id, temp.name, temp.surname, temp.city);
      }
      else
        return HP_ERROR; //Somebody gave us a value, which doesn't exist.
    }
  }  
  CALL_BF(BF_UnpinBlock(block)); //Block has just unpinned.
}
  
BF_Block_Destroy(&block);
return HP_OK;
}

HP_ErrorCode HP_GetEntry(int fileDesc, int rowId, Record *record) {
int records_per_block = BF_BLOCK_SIZE/sizeof(Record); //512/60=8
int the_block = (rowId-1)/records_per_block + 1;
int the_record = rowId%records_per_block;  
//printf("%d %d %d \n", records_per_block,the_block,the_record);
BF_Block* block;
BF_Block_Init(&block);

CALL_BF(BF_GetBlock(fileDesc,the_block,block));
char* data = BF_Block_GetData(block); //Block has just pinned.
data+=sizeof(int);
if(the_record==0)
  memcpy(record,data+(records_per_block-1)*sizeof(Record),sizeof(Record));
else
  memcpy(record,data+(the_record-1)*sizeof(Record),sizeof(Record));

CALL_BF(BF_UnpinBlock(block)); //Block has just unpinned.
BF_Block_Destroy(&block);

return HP_OK;
}