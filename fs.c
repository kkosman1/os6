#include "fs.h"
#include "disk.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <math.h>

#define DISK_BLOCK_SIZE    4096
#define FS_MAGIC	   0xf0f03410
#define INODES_PER_BLOCK   128
#define POINTERS_PER_INODE 5
#define POINTERS_PER_BLOCK 1024

int MOUNT=0;
int *INODE_BITMAP;
int *BLOCK_BITMAP;

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
	if (MOUNT==1){
		printf("Error: cannot mount before formatting.\n");
		return 0;
	}

	union fs_block block;
	struct fs_inode blankinode = {0,0,{0,0,0,0,0},0};
	int i;	
		
	for (i=0; i<POINTERS_PER_INODE; i++){
		block.inode[i]=blankinode;
	}
		
	block.super.magic = FS_MAGIC;
	block.super.nblocks = disk_size();
	block.super.ninodeblocks = block.super.nblocks*.10 + 1;
	block.super.ninodes = INODES_PER_BLOCK * block.super.ninodeblocks;  //total
	
	disk_write(0, block.data);
	return 1;
}

void fs_debug()
{
	int i,j,k,m;
	int size;
	int numblock;
	int direct;
	union fs_block block, indirectblock;
	disk_read(0,block.data);

	printf("superblock:\n");
	if (block.super.magic != FS_MAGIC){
		printf("Error: magic number is invalid.\n");
		return;
	}
	else{
		printf("Magic number is valid.\n");	
	}
	printf("    %d blocks\n",block.super.nblocks);
	printf("    %d inode blocks\n",block.super.ninodeblocks);
	printf("    %d inodes total \n",block.super.ninodes);
	
	int ninodes = block.super.ninodeblocks;
	for (i=1; i<=ninodes; i++){
		disk_read(i,block.data);
		for (j=0;j<128;j++){
			if (block.inode[j].isvalid == 1){
				printf("inode: %d \n", j);
				printf("    size %d bytes \n", block.inode[j].size);
				printf("    direct blocks: ");
				size = block.inode[j].size;
				numblock = ceil((double)size/(double)DISK_BLOCK_SIZE); //get upper limit of division
				if (numblock >= POINTERS_PER_INODE)
					direct=POINTERS_PER_INODE;
				else
					direct=numblock;
				numblock=numblock-direct;
				for (k=0;k<direct;k++){
					printf("%d ", block.inode[j].direct[k]);
				}	
				if (numblock>0){
					//go to the indirect block which contains a list of pointers
					printf("\n    indirect block: %d\n", block.inode[j].indirect);
					disk_read(block.inode[j].indirect, indirectblock.data);
					printf("    indirect data blocks: ");
					for (m=0;m<numblock;m++) {
						printf("%d ", indirectblock.pointers[m]);
					}
				}
				printf("\n");
			}
		}
	}
}


int fs_mount()
{
	if (MOUNT==1){
		printf("Error: already mounted disk.\n");
		return 0;
	}

	union fs_block block, indirectblock;
	disk_read(0,block.data);

	//Check magic number
	if (block.super.magic != FS_MAGIC){
		printf("Error: magic number is invalid");
		return 0;
	}

	//Set up the bitmap
	INODE_BITMAP = malloc(block.super.ninodes * sizeof(int));
	BLOCK_BITMAP = malloc(block.super.nblocks * sizeof(int));

	int i,j,k,m;
	
	//initialize block bitmap to 0
	for (i=0;i<block.super.nblocks;i++)
		BLOCK_BITMAP[i]=0;

	for (i=0;i<block.super.ninodes;i++){
		INODE_BITMAP[i]=0;
	}

	int numblock, size, direct;

	int ninodes = block.super.ninodeblocks;
	for (i=1; i<=ninodes; i++){
		BLOCK_BITMAP[i]=1;
		disk_read(i,block.data);
		for (j=0;j<INODES_PER_BLOCK;j++){
			if (block.inode[j].isvalid == 1){
				INODE_BITMAP[j]=1;
				size = block.inode[j].size;
				numblock = ceil((double)size/(double)DISK_BLOCK_SIZE); //get upper limit of division
				if (numblock >= POINTERS_PER_INODE)
					direct=POINTERS_PER_INODE;
				else
					direct=numblock;
				numblock=numblock-direct;
				for (k=0;k<direct;k++){
					BLOCK_BITMAP[block.inode[j].direct[k]]=1;
				}
				if (numblock>0){
					//go to the indirect block which contains a list of pointers
					disk_read(block.inode[j].indirect, indirectblock.data);
					BLOCK_BITMAP[block.inode[j].indirect]=1;
					for (m=0;m<numblock;m++) {
						BLOCK_BITMAP[indirectblock.pointers[m]]=1;
					}
				}
			}
		}
	}

	MOUNT=1;
	return 1;
}

int fs_create()
{
	if (MOUNT==0){
		printf("Error: cannot create before mounting\n");
		return 0;
	}
	union fs_block block;
	disk_read(0,block.data);
	int i;
	struct fs_inode blankinode = {0,0,{0,0,0,0,0},0};
	blankinode.isvalid=1;

	for (i=1;i<=block.super.ninodes;i++){
		if (INODE_BITMAP[i] == 0){
			INODE_BITMAP[i]=1;
			int blocknum = i/127 + 1;
			disk_read(blocknum, block.data);
			block.inode[i] = blankinode;
			disk_write(blocknum, block.data);
			return i;
		}
	}
	printf("Error: no inodes available\n");
	return 0;
}

int fs_delete( int inumber )
{
	if (MOUNT==0){
		printf("Error: cannot delete before mounting.\n");
		return 0;
	}
	int blocknum = inumber/127 + 1;
	int k,m;
	int size, numblock, direct;
	union fs_block block, indirectblock;
	disk_read(blocknum,block.data);

	if (block.inode[inumber].isvalid == 1){
		size = block.inode[inumber].size;
		numblock = ceil((double)size/(double)DISK_BLOCK_SIZE); //get upper limit of division
		if (numblock >= POINTERS_PER_INODE)
			direct=POINTERS_PER_INODE;
		else
			direct=numblock;
		numblock=numblock-direct;
		for (k=0;k<direct;k++){
			BLOCK_BITMAP[block.inode[inumber].direct[k]]=0;
		}
		if (numblock>0){
			//go to the indirect block which contains a list of pointers
			disk_read(block.inode[inumber].indirect, indirectblock.data);
			for (m=0;m<numblock;m++) {
		       		BLOCK_BITMAP[indirectblock.pointers[m]]=0;
			}
			BLOCK_BITMAP[block.inode[inumber].indirect]=0;
		}
	}
	else{
		printf("Error: cannot delete node, does not exist");
		return 0;
	}
	struct fs_inode blankinode={0,0,{0,0,0,0,0},0};
	block.inode[inumber]=blankinode;
	disk_write(blocknum, block.data);

	return 1;
}

int fs_getsize( int inumber )
{
	int blocknum = inumber/127 + 1;
	union fs_block block;
	disk_read(blocknum,block.data);
	if (block.inode[inumber].isvalid == 0){
		printf("Error: invalid inumber\n");
		return -1; //check if vaild first
	}
	return block.inode[inumber].size;
}

int fs_read( int inumber, char *data, int length, int offset )
{
	if (MOUNT == 0){
		printf("Error: not mounted. \n");
		return 0;
	}

	int blocknum = inumber/127 + 1;
	union fs_block block, datablock, indirectblock;
	disk_read(blocknum,block.data);
	if (INODE_BITMAP[inumber]==0){
		printf("Error: invalid inumber.\n");
		return 0;
	}

	int i;
	int size = block.inode[inumber].size;
	int nCounter=offset/DISK_BLOCK_SIZE;
	int end;

	if (size <= offset)
		return 0;
	else if (size > (length+offset))
		end = length+offset;
	else
		end = size;

	int numblock = ceil((double)end/(double)DISK_BLOCK_SIZE); //get upper limit of division
	int remainder = end % DISK_BLOCK_SIZE;
	int index=0;
	int direct, readsize;	

	if (numblock > POINTERS_PER_INODE)
		direct = POINTERS_PER_INODE;
	else
		direct = numblock;

	for (i=nCounter;i<direct;i++){
		disk_read(block.inode[inumber].direct[i], datablock.data);
		if (i==(numblock-1))
			readsize=remainder;
		else
			readsize=DISK_BLOCK_SIZE;
		memcpy(data+index,datablock.data,readsize);
		index=index+readsize;
	}
	int start;
	if (nCounter>=5)
		start=nCounter;
	else
		start=0;
	int indirect = numblock-direct;
	disk_read(block.inode[inumber].indirect, indirectblock.data);
	for (i=start;i<indirect;i++){
		disk_read(indirectblock.pointers[i], datablock.data);
		if ((i+direct)==(numblock-1))
			readsize=remainder;
		else
			readsize=DISK_BLOCK_SIZE;
		memcpy(data+index,datablock.data,readsize);
		index=index+readsize;
	}
	return index;
}

//do last
int fs_write( int inumber, const char *data, int length, int offset )
{
	if (MOUNT == 0){
		printf("Error: not mounted. \n");
		return 0;
	}

	if (offset > sizeof(data))
		return 0;

	union fs_block block, indirectblock, datablock;
	disk_read(0,block.data);

	int i, readsize;
	int index=offset;
	 
	int blocknum = inumber/127 + 1;
	int bitmapSize=block.super.nblocks;

	disk_read(blocknum,block.data);
	
	int size = block.inode[inumber].size;
	int count, icount;
	int currBlocks = ceil((double)size/(double)DISK_BLOCK_SIZE); //blocks used currently
	int currRemainder = size % DISK_BLOCK_SIZE;
	int numblock = ceil((double)(length+size)/(double)DISK_BLOCK_SIZE); //last block number to be accessed
	int remainder = (length+size) % DISK_BLOCK_SIZE;

	printf("length: %d\nsize: %d\ncurrRemainder: %d\n", length, size, currRemainder);

	if (INODE_BITMAP[inumber] == 0){
		printf("Error: invalid inumber.\n");
		return 0;
	}
	count = currBlocks;
	if (currBlocks < 5){
		icount = 1;
	}
	else {
		icount = currBlocks-5; //start indirect
	}

	disk_read(block.inode[inumber].indirect, indirectblock.data);

	if (currRemainder > 0){ //fill in end of a block
		if (count==numblock){
			readsize=remainder-currRemainder;
		}
		else
			readsize=DISK_BLOCK_SIZE-currRemainder;
		if (currBlocks<=5){
			disk_read(block.inode[inumber].direct[currBlocks-1], datablock.data);
			memcpy(datablock.data+currRemainder, data+index, readsize);
			disk_write(block.inode[inumber].direct[currBlocks-1], datablock.data);
		}
		else {
			disk_read(indirectblock.pointers[currBlocks-6], datablock.data);
			memcpy(datablock.data+currRemainder, data+index, readsize);
			disk_write(indirectblock.pointers[currBlocks-6], datablock.data);
		}
		index=index+readsize;
		if (count==numblock){
			block.inode[inumber].size+=index;
			disk_write(blocknum, block.data);
			disk_write(block.inode[inumber].indirect, indirectblock.data);
			return index-offset;
		}			
	}

	for (i=0; i<bitmapSize; i++){
		if (BLOCK_BITMAP[i] == 0){
			if (count==numblock){
				readsize=remainder;
				printf("using remainder    ");
			}
			else
				readsize=DISK_BLOCK_SIZE;
			disk_read(i, datablock.data);
			memcpy(datablock.data, data+index, readsize);
			if (count<=5){ //direct
				block.inode[inumber].direct[count-1]=i;
				disk_write(i, datablock.data);
			}
			else if (icount <= POINTERS_PER_BLOCK){ //indirect
				indirectblock.pointers[icount]=i;
				icount++;
				disk_write(i, datablock.data);
			}
			else {
				break;
			}
	    		index=index+readsize;
			BLOCK_BITMAP[i]=1;
			if (count==numblock){
				block.inode[inumber].size+=index;
				disk_write(blocknum, block.data);
				disk_write(block.inode[inumber].indirect, indirectblock.data);
				return index-offset;
			}
			count++;
		}
	}

	block.inode[inumber].size+=index;	
	disk_write(blocknum, block.data);
	disk_write(block.inode[inumber].indirect, indirectblock.data);
	return index-offset;
}
