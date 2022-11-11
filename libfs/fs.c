#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdint.h>

#include "disk.h"
#include "fs.h"

#define EOC 0xFFFF

#define disk_error(fmt, ...) \
	fprintf(stderr, "%s: "fmt"\n", __func__, ##__VA_ARGS__)

/* TODO: Phase 1 */

struct __attribute__((__packed__)) superblock
{
	char signature[8]; //(8 characters) signature
	uint16_t numBlockVirtualDisk; //(2 bytes) total amount of blocks of virtual disk
	uint16_t rootBlockIndex; //(2 bytes) root directory block index
	uint16_t dataBlockStartIndex; //(2 bytes) data block start index
	uint16_t numDataBlocks;// (2 bytes)  amount of data blcoks
	uint8_t numBlocksFAT; //(1 bytes)// number of blocks for FAT
	uint8_t padding[4079]; //(4079 bytes)// unsused/padding  ASK THIS 
	
};


struct __attribute__((__packed__)) FAT
{
	uint16_t next;
	
};

struct __attribute__((__packed__)) rootdirectory
{
	
	char fileName[FS_FILENAME_LEN]; //(16 bytes) Filename
	uint32_t sizeOfFile; //(4 bytes) Size of the files (in bytes)
	uint16_t firstIndex; //(2 bytes) Index of the first data block
	uint8_t padding[10]; //(10 bytes) Unused/Padding
	
};

//creating objects of structs
struct superblock *superBlock;
struct FAT *fatArray;
struct rootdirectory *rootDirectory;
/**
 * fs_mount - Mount a file system
 * @diskname: Name of the virtual disk file
 *
 * Open the virtual disk file @diskname and mount the file system that it
 * contains. A file system needs to be mounted before files can be read from it
 * with fs_read() or written to it with fs_write().
 *
 * Return: -1 if virtual disk file @diskname cannot be opened, or if no valid
 * file system can be located. 0 otherwise.
 */

int fs_mount(const char *diskname)
{
	/* TODO: Phase 1 */
	/*open the disk */
	/*
	fs_mount() makes the file system contained 
	in the specified virtual disk “ready to be used”.
	You need to open the virtual disk, using the 
	block API, and load the meta-information that is
	necessary to handle the file system operations
	described in the following phases. 


	Don’t forget that your function fs_mount() 
	should perform some error checking in order 
	to verify that the file system has the expected 
	format. For example, the signature of the file
	system should correspond to the one defined by
	the specifications, the total amount of block
	should correspond to what block_disk_count()
	returns, etc.
	*/

	//open disk
	//block_disk_open(const char *diskname);
	//if virtual file disk cannot be openned return -1;
	if (block_disk_open(diskname) == -1)
	{
		disk_error("disk cannot be opened");
		return -1;
	}

	//declaring a super block
	superBlock = malloc(sizeof(BLOCK_SIZE));
	if (block_read(0,(void *)superBlock) == -1)
	{
		disk_error("Cannot read from block");
		return -1;
	}

	//checking signature
	if (strcmp(superBlock->signature, "ECS150FS") != 0){
		disk_error("Signature not matched");
		return -1;
	}

	//block's disk count check
	if (block_disk_count() != superBlock->numBlockVirtualDisk){
		disk_error("Number of blocks not matched");
		return -1;
	}
	
	//fatArray initialization
	fatArray = malloc(sizeof(superBlock->numBlocksFAT * BLOCK_SIZE));
	for (int i = 0; i < superBlock->numBlocksFAT; i++)
	{
		if (block_read(i + 1, (void *)fatArray + (i*BLOCK_SIZE))== -1)
		{
			disk_error("Error in reading");
			return -1;
		}

	}

	//rootDirectory initialization
	rootDirectory = malloc(sizeof(struct rootdirectory));
	if (block_read(superBlock->rootBlockIndex,(void *)rootDirectory) == -1)
	{
		disk_error("Cannot read from root directory block");
		return -1;
	}
	
	return 0;
}

int fs_umount(void)
{
	/* TODO: Phase 1 */
	return 0;
}

int fs_info(void)
{
	/* TODO: Phase 1 */
	return 0;
}

int fs_create(const char *filename)
{
	/* TODO: Phase 2 */
	return 0;
}

int fs_delete(const char *filename)
{
	/* TODO: Phase 2 */
	return 0;
}

int fs_ls(void)
{
	/* TODO: Phase 2 */
	return 0;
}

int fs_open(const char *filename)
{
	/* TODO: Phase 3 */
	return 0;
}

int fs_close(int fd)
{
	/* TODO: Phase 3 */
	return 0;
}

int fs_stat(int fd)
{
	/* TODO: Phase 3 */
	return 0;
}

int fs_lseek(int fd, size_t offset)
{
	/* TODO: Phase 3 */
	return 0;
}

int fs_write(int fd, void *buf, size_t count)
{
	/* TODO: Phase 4 */
	return 0;
}

int fs_read(int fd, void *buf, size_t count)
{
	/* TODO: Phase 4 */
	return 0;
}
