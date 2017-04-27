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
		
		for (int i=0; i<POINTERS_PER_INODE; i++){
			if (block.inode[i].isvalid == 1){
				fs_delete(block.inode[i]); //get inumber and delete it
			}
		}
		
		superblock.magic = FS_MAGIC;
		superblock.nblocks = DISK_BLOCK_SIZE;
		superblock.ninodeblocks = DISK_BLOCK_SIZE*.10;
		superblock.niodes = INODES_PER_BLOCK * DISK_BLOCK_SIZE*.10;  //total                      
	
		disk_write(0, superblock);
		
		return 1;
	}
}

void fs_debug()
{
	int i;
	int j;
	
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
	
	for (i=0; i<INODES_PER_BLOCK-1; i++){
		if (block.inode[i].isvalid == 1){
			printf("inode: %d bytes \n", block.inode[i]);
			printf("    size %d \n", block.inode[i].size);
			printf("    direct blocks: ");
			for (j=0; j<POINTERS_PER_INODE; j++){ 
				printf(" %d ", block.inode[i].direct[j]);
			}
			printf("\n");
		}
	}
	i=i+1;
	if (block.inode[i].isvalid == 1){
		printf("inode: %d bytes \n", block.inode[i]);
		printf("    size %d \n", block.inode[i].size);
		printf("    direct blocks: ");
		for (j=0; j<POINTERS_PER_INODE; j++){
			printf(" %d ", block.inode[i].direct[j]);
		}
		printf("\n");
		if (block.inode[i].indirect!=0){
			printf("    indirect block: %d \n", block.inode[i].indirect);
			printf("    indirect data blocks: ");
			union fs_block indirectblock;
			disk_read(block.inode[i], indirectblock.data);
			for (j=0; j<POINTERS_PER_BLOCK; j++){
				if (indirectblock.pointers[i] != 0){
					printf(" %d ", indirectblock.pointers[i]);
				}
			}
			printf("\n");
		}
	}		
}

int fs_mount()
{
	//look for the magic number
	//search for FS_MAGIC
//	if(canFind(FS_MAGIC)){
	
//	}
//	else{
		
//	}
	return 0;
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
	return 0;
}

int fs_getsize( int inumber )
{
	return -1;
}

int fs_read( int inumber, char *data, int length, int offset )
{
	
	return 0;
}

//do last
int fs_write( int inumber, const char *data, int length, int offset )
{
	return 0;
}
