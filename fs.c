#include "fs.h"
#include "disk.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>

#define DISK_BLOCK_SIZE    4096
#define FS_MAGIC	   0xf0f03410
#define INODES_PER_BLOCK   128
#define POINTERS_PER_INODE 5
#define POINTERS_PER_BLOCK 1024

struct fs_superblock {
	int magic;
	int nblocks;
	int ninodeblocks;
	int ninodes;
};

struct fs_inode {
	int isvalid;
	int size;
	int direct[POINTERS_PER_INODE];
	int indirect;
};

union fs_block {
	struct fs_superblock super;
	struct fs_inode inode[INODES_PER_BLOCK];
	int pointers[POINTERS_PER_BLOCK];
	char data[DISK_BLOCK_SIZE];
};

//Set aside 10 percent of the blocks for inode, clears iniode table, write
//to the superblock
int fs_format()
{
	union fs_block block;
	struct fs_superblock superblock;
	int i;
		
	disk_read(0, block.data);
		
	for (int i=1; i<POINTERS_PER_INODE; i++){
		if (block.inode[i].isvalid == 1){
			fs_delete(block.inode[i]); //get inumber and delete it
		}
	}
		
	superblock.magic = FS_MAGIC;
	superblock.nblocks = DISK_BLOCK_SIZE;
	superblock.ninodeblocks = DISK_BLOCK_SIZE*.10;
	superblock.niodes = INODES_PER_BLOCK * DISK_BLOCK_SIZE*.10;  //total		      
	
	if (disk_write(0, superblock)){
		return 1;
	}
	else{
		return 0; //if fails to write, return 0
	}
}

void fs_debug()
{
	int i;
	int j;
	int size;
	int extra=0;
	int numinodes=0;
	int nCounter=0;
	
	union fs_block block;

	disk_read(0,block.data);

	printf("superblock:\n");
	if (block.super.magic == FS_MAGIC){
		printf("magic number is valid");
	}
	else{
		printf("Error: magic number is invalid");
		return;
	}
	printf("    %d blocks\n",block.super.nblocks);
	printf("    %d inode blocks\n",block.super.ninodeblocks);
	printf("    %d inodes total \n",block.super.ninodes);
	
	for (i=0; i<=block.super.ninodes; i++){
		if (block.inode[i].isvalid == 1){
			printf("inode: %d \n", block.inode[i]);
			printf("    size %d bytes \n", block.inode[i].size);
			printf("    direct blocks: ");
			size = block.inode[1];
			nCounter=0;
			numblock = (size + (DISK_BLOCK_SIZE-1))/ DISK_BLOCK_SIZE; //get upper limit of division
			remainder = size % DISK_BLOCK_SIZE;		
			while(nCounter < numblocks){
				//go until it is equal to number of direct blocks - 1
				if (nCounter < numblocks-1){
					printf(" %d ", block.inode[i].direct[nCounter]);
				}
				//if you run into partial on the last one
				else if(size < (nCounter*DISK_BLOCK_SIZE) && (nCounter == numblocks -1)){
					printf(" %d ", block.inode[i].direct[nCounter]);
				}
				//if you run into indirect on last one
				else if (size > (nCounter*DISK_BLOCK_SIZE) && (nCounter == numblocks -1)){
					nCounter=0; //start back to beginnign for indirect block indexing
					extra = size - (nCounter*DISK_BLOCK_SIZE)
					union fs_block indirectblock;
					//go to the indirect block which contains a list of pointers
					disk_read(block.inode[i].direct[nCounter], indirectblock.data);
					printf("    indirect block: %d \n", block.inode[i].direct[nCounter]);
					//either direct[nCounter] or indirect
					printf("    indirect block: %d \n", block.inode[i].indirect);
					printf("    indirect data blocks: ");
					while (extra > 0){
						printf(" %d ", indirectblock.pointers[nCounter]);
						nCounter++;
						extra=extra-DISK_BLOCK_SIZE;
					}
				}
				nCounter++;
				printf("\n");
			}
			printf("\n");
		}
	}
}


int fs_mount()
{
	unsigned char *bitmap;
	int spaceNeeded;

	union fs_block block;
	disk_read(0,block.data);

	if (block.super.magic == FS_MAGIC){
		printf("magic number is valid");
	}
	else{
		printf("Error: magic number is invalid");
		return 0;
	}
	//Set up the bitmap
	spaceNeeded=DISK_BLOCK_SIZE*8; //number of disk blocks needed
	bitmap = malloc(DISK_BLOCK_SIZE);
	//Prepare file system for use (set valid to 1?)
}

int fs_create()
{
	fs_inode iNode;
	iNode.length=0;
	iNode.isvalid=1;
	return iNode;
}

int fs_delete( int inumber )
{
	int extra = 0;
	int size = 0;
	int numblock;
	int remainder;
	int nCounter;
	
	union fs_block block;
	disk_read(0,block.data);
	if (block.inode[inumber].isvalid == 0){
		return 0; //check if vaild first
	}
	
	//loop through and free all of the data
	free(block.inode[inumber]);
	size = block.inode[1];
	nCounter=0;
	numblock = (size + (DISK_BLOCK_SIZE-1))/ DISK_BLOCK_SIZE; //get upper limit of divisi$
	remainder = size % DISK_BLOCK_SIZE;
	while(nCounter < numblocks){		
		//if on the last on and it has an indirect block
		if (size > (nCounter*DISK_BLOCK_SIZE) && (nCounter == numblocks -1)){
			nCounter=0;
			
			//go into direct and free those
			extra = size - (nCounter*DISK_BLOCK_SIZE)
			union fs_block indirectblock;
			
			//go to the indirect block which contains a list of pointers
			disk_read(block.inode[i].direct[nCounter], indirectblock.data);
			while (extra > 0){
				free(indirectblock.pointers[nCounter]);
				nCounter++;
				extra=extra-DISK_BLOCK_SIZE;
			}
		}
		else if (nCounter < numblocks-1){
			free(block.inode[i].direct[nCounter]);
		}		
		nCounter++;
	}
	return 1;
}

int fs_getsize( int inumber )
{
	union fs_block block;
	disk_read(0,block.data);
	if (block.inode[inumber].isvalid == 0){
		return -1; //check if vaild first
	}
	return block.inode[inumber].size();
}

int fs_read( int inumber, char *data, int length, int offset )
{
	int byteCount=0;
	union fs_block block;
	disk_read(0,block.data);
	if (block.inode[inumber].isvalid == 0){
		return 0; //check if vaild first
	}
  
	memcpy(offset, data, length);

	byteCount = read(offset, data, length);
	//lseek(inumber, offset, SEEK_SET); 
	//byteCount = read(inumber, data, length);	
	return byteCount;
}

//do last
int fs_write( int inumber, const char *data, int length, int offset )
{
	int byteCount=0;
	union fs_block block;
	disk_read(0,block.data);
	if (block.inode[inumber].isvalid == 0){
		return 0; //check if vaild first
	}						
	malloc((sizeof(data));
	memcpy(offset, data, length);
	byteCount = write(offset, data, length);
	return 0;
}
