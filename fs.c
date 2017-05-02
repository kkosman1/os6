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
char *INODE_BITMAP;
char *BLOCK_BITMAP;

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
		printf("Error: cannot format before mounting.\n");
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
	block.super.ninodeblocks = block.super.nblocks*.10;
	block.super.ninodes = INODES_PER_BLOCK * block.super.ninodeblocks;  //total
	
	disk_write(0, block.data);
	return 1;
}

void fs_debug()
{
	int i;
	int size;
	int nCounter=0;
	int numblock=0;
	
	union fs_block block;

	disk_read(0,block.data);

	printf("superblock:\n");
	if (block.super.magic != FS_MAGIC){
		printf("Error: magic number is invalid.\n");
		return;
	}
	printf("    %d blocks\n",block.super.nblocks);
	printf("    %d inode blocks\n",block.super.ninodeblocks);
	printf("    %d inodes total \n",block.super.ninodes);
	
	for (i=1; i<block.super.ninodes; i++){
		if (block.inode[i].isvalid == 1){
			printf("inode: %d \n", i);
			printf("    size %d bytes \n", block.inode[i].size);
			printf("    direct blocks: ");
			size = block.inode[i].size;
			nCounter=0;
			numblock = ceil(size/DISK_BLOCK_SIZE); //get upper limit of division
			int indirect = -1;
			union fs_block indirectblock;
			while(nCounter < numblock){
				if (block.inode[i].direct[nCounter]!=0){
					printf("%d ", block.inode[i].direct[nCounter]);
				}	
				else if (indirect==-1){
					indirect++;
                                        //go to the indirect block which contains a list of pointers
                                        disk_read(block.inode[i].indirect, indirectblock.data);
                                        printf("\n    indirect block: %d\n", block.inode[i].indirect);
					if (nCounter!=numblock-1)
						printf("    indirect data blocks: ");
				}
				else {
					printf("%d ", indirectblock.pointers[indirect]);
					indirect++;
				}
				nCounter++;
			}
			printf("\n");
		}
	}
}


int fs_mount()
{
	if (MOUNT==1){
		printf("Error: already mounted disk.\n");
		return 0;
	}

	union fs_block block;
	disk_read(0,block.data);

	//Check magic number
	if (block.super.magic != FS_MAGIC){
		printf("Error: magic number is invalid");
		return 0;
	}

	//Set up the bitmap
	INODE_BITMAP = malloc(block.super.ninodes+1 * sizeof(char));
	BLOCK_BITMAP = malloc(block.super.nblocks * sizeof(char));

	//fill inode and block bitmaps (set free to 0)
	int i;

	//initialize block bitmap to 0
	for (i=0;i<block.super.nblocks;i++)
		BLOCK_BITMAP[i]='0';

	int numblock, nCounter, size;

	for (i=1;i<=block.super.ninodes; i++){
		if (block.inode[i].isvalid==1){ //set inode bitmap
			INODE_BITMAP[i]='1';

                        size = block.inode[i].size;
                        nCounter=0;
                        numblock = (size/DISK_BLOCK_SIZE); //get upper limit of division
                        int indirect = -1;
                        union fs_block indirectblock;
                        while(nCounter < numblock){
                                if (BLOCK_BITMAP[nCounter]==1){
                                        BLOCK_BITMAP[block.inode[i].direct[nCounter]]='1';
                                }
                                else if (indirect==-1){
                                        indirect++;
                                        //go to the indirect block which contains a list of pointers
                                        disk_read(block.inode[i].indirect, indirectblock.data);
					BLOCK_BITMAP[block.inode[i].indirect]= '1';
				}
                                else {
                                        BLOCK_BITMAP[indirectblock.pointers[indirect]]='1';
                                        indirect++;
                                }
                                nCounter++;
                        }
                }
		else{
                        INODE_BITMAP[i]='0';
		}
        }
	MOUNT=1;
	return 1;
}

int fs_create()
{
	union fs_block block;
	disk_read(0,block.data);
	int i;
	for (i=1;i<=block.super.ninodes;i++){
		if (INODE_BITMAP[i]=='0'){
			INODE_BITMAP[i]='1';
			block.inode[i].size=0;
			block.inode[i].isvalid=1;
			return i;
		}
	}
	return 0;
}

int fs_delete( int inumber )
{
	union fs_block block;
	disk_read(0,block.data);

	if (block.inode[inumber].isvalid == 1){
        	int size = block.inode[inumber].size;
                int nCounter=0;
                int numblock = ceil(size/DISK_BLOCK_SIZE); //get upper limit of division
                int indirect = -1;
                union fs_block indirectblock;
                while(nCounter < numblock){
                	if (BLOCK_BITMAP[nCounter]==1){
				BLOCK_BITMAP[block.inode[inumber].direct[nCounter]]='0';
                        }
                        else if (indirect==-1){
                	        indirect++;
                                //go to the indirect block which contains a list of pointers
                                disk_read(block.inode[inumber].indirect, indirectblock.data);
				BLOCK_BITMAP[block.inode[inumber].indirect]='0';
                        }
			else {
                                BLOCK_BITMAP[indirectblock.pointers[indirect]]='0';
                                indirect++;
                        }
                        nCounter++;
		}
	}
	return 1;
}

int fs_getsize( int inumber )
{
	union fs_block block;
	disk_read(0,block.data);
	if (block.inode[inumber].isvalid == 0){
		printf("Error: invalid inumber\n");
		return -1; //check if vaild first
	}
	return block.inode[inumber].size;
}

int fs_read( int inumber, char *data, int length, int offset )
{
	union fs_block block;
	disk_read(0,block.data);
	if (block.inode[inumber].isvalid==0){
		printf("Error: invalid inumber.\n");
		return 0;
	}
	int i;
	int size = block.inode[inumber].size;
        int nCounter=offset/DISK_BLOCK_SIZE;
	if (size > (length+offset))
                size=length+offset;
        int numblock = ceil(size/DISK_BLOCK_SIZE); //get upper limit of division
	int remainder = size % DISK_BLOCK_SIZE;
        int indirect = -1;
        union fs_block indirectblock;
        while(nCounter < (numblock)){
        	if (BLOCK_BITMAP[nCounter]==1){
			if (nCounter==numblock-1) //last one--use remainder
				strcat(data, block.data[nCounter][0:remainder]);
			else
                		strcat(data, block.data[nCounter]);
                }
                else if (indirect==-1){
                        indirect++;
                        //go to the indirect block which contains a list of pointers
                        disk_read(block.inode[inumber].indirect, indirectblock.data);
			if (nCounter==numblock-1) //last one--use remainder
                                strcat(data, block.data[nCounter][0:remainder]);
                        else
                                strcat(data, block.data[nCounter]);
                }
                else {
			if (nCounter==numblock-1) //last one--use remainder
                                strcat(data, indirectblock.data[indirect][0:remainder]);
                        else
                                strcat(data, indirectblock.data[indirect]);
                        indirect++;
		}                
                nCounter++;
	}
	return size-offset;
}

//do last
int fs_write( int inumber, const char *data, int length, int offset )
{
/*	int byteCount=0;
	union fs_block block;
	disk_read(0,block.data);
	if (block.inode[inumber].isvalid == 0){
		return 0; //check if vaild first
	}						
	malloc(sizeof(data));
	memcpy(offset, data, length);
	byteCount = write(offset, data, length);
*/	return 0;
}
