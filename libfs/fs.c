#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "disk.h"
#include "fs.h"

#define FAT_EOC 0xFFFF
#define FAT_FREE 0

#define disk_error(fmt, ...) \
	fprintf(stderr, "%s: " fmt "\n", __func__, ##__VA_ARGS__)

/* TODO: Phase 1 */

struct __attribute__((__packed__)) superblock
{
	char signature[8];			  //(8 characters) signature
	uint16_t numBlockVirtualDisk; //(2 bytes) total amount of blocks of virtual disk
	uint16_t rootBlockIndex;	  //(2 bytes) root directory block index
	uint16_t dataBlockStartIndex; //(2 bytes) data block start index
	uint16_t numDataBlocks;		  // (2 bytes)  amount of data blcoks
	uint8_t numBlocksFAT;		  //(1 bytes)// number of blocks for FAT
	uint8_t padding[4079];		  //(4079 bytes)// unsused/padding  ASK THIS
};

struct __attribute__((__packed__)) FAT
{
	uint16_t next;
};

struct __attribute__((__packed__)) rootdirectory
{

	char fileName[FS_FILENAME_LEN]; //(16 bytes) Filename
	uint32_t sizeOfFile;			//(4 bytes) Size of the files (in bytes)
	uint16_t firstIndex;			//(2 bytes) Index of the first data block
	uint8_t padding[10];			//(10 bytes) Unused/Padding
};

// creating objects of structs
struct superblock *superBlock;
struct FAT *fatArray;
struct rootdirectory *rootDirectory;
bool disk_open = false;


 //HELPER FUNCTIONS
// int countOfRootDirectoryFiles(struct *rootdirectory){
// 	// int occupiedFileCount = 0;
// 	int freeRootDirectoryCount = 0;
// 	for (int i = 0; i < FS_FILE_MAX_COUNT; i++)
// 	{
// 		if (rootDirectory[i].sizeOfFile == 0)
// 		{
// 			freeRootDirectoryCount++;
// 		}
// 	}
// 	int occupiedFileCount = FS_FILE_MAX_COUNT - freeRootDirectoryCount;
// 	return occupiedFileCount;

// }
 

int fs_mount(const char *diskname)
{
	// open disk
	// if virtual file disk cannot be openned return -1;
	if (block_disk_open(diskname) == -1)
	{
		disk_error("disk cannot be opened");
		return -1;
	}
	disk_open = true;

	// declaring a super block
	superBlock = malloc(BLOCK_SIZE);
	if (superBlock == NULL)
	{
		perror("Mem superblock fail");
		return -1;
	}

	if (block_read(0, (void *)superBlock) == -1)
	{
		disk_error("Cannot read from block");
		return -1;
	}

	// checking signature
	if (strncmp(superBlock->signature, "ECS150FS", 8) != 0)
	{
		disk_error("Signature not matched");
		return -1;
	}

	if (block_disk_count() != superBlock->numBlockVirtualDisk)
	{
		disk_error("Number of blocks not matched");
		return -1;
	}

	// fatArray initialization
	fatArray = malloc(superBlock->numBlocksFAT * BLOCK_SIZE);
	for (int i = 0; i < superBlock->numBlocksFAT; i++)
	{
		if (block_read(i + 1, (void *)fatArray + (i * BLOCK_SIZE)) == -1)
		{
			disk_error("Error in reading");
			return -1;
		}
	}

	// rootDirectory initialization
	rootDirectory = malloc(sizeof(struct rootdirectory) * FS_FILE_MAX_COUNT);
	if (block_read(superBlock->numBlocksFAT + 1, (void *)rootDirectory) == -1)
	{
		disk_error("Cannot read from root directory block");
		return -1;
	}

	return 0;
}

/**
 * fs_umount - Unmount file system
 *
 * Unmount the currently mounted file system and close the underlying virtual
 * disk file.
 *
 * Return: -1 if no FS is currently mounted, or if the virtual disk cannot be
 * closed, or if there are still open file descriptors. 0 otherwise.
 */
/*
	This means that whenever fs_umount() is called, all meta-information
	and file data must have been written out to disk.
*/
int fs_umount(void)
{
	/* If no disk is opened, return -1 */
	if (disk_open == false)
	{
		disk_error("No disk opened");
		return -1;
	}
	/*Error: No FS is currently mounted */
	if (!superBlock)
	{
		disk_error("No FS mounted");
		return -1;
	}

	// Writing metadata to the disk
	if (block_write(0, (void *)superBlock) == -1)
	{
		disk_error("Error in writing metadata to disk");
		return -1;
	}

	// Writing file data to disk

	for (int i = 0; i < superBlock->numBlocksFAT; i++)
	{
		if (block_write(i + 1, (void *)fatArray + (i * BLOCK_SIZE)) == -1)
		{
			disk_error("Error in writing FAT blocks to disk");
			return -1;
		}
	}

	if (block_write(superBlock->numBlocksFAT + 1, (void *)rootDirectory) == -1)
	{
		disk_error("Error in writing root directory blocks to disk");
		return -1;
	}

	/*Error: There are file descriptors still open: STILL NEED TO IMPLEMENT*/

	free(superBlock);
	free(fatArray);
	free(rootDirectory);

	/*Error: Virtual disk can not be closed */
	if (block_disk_close() < 0)
	{
		disk_error("Virtual disk can not be closed");
		return -1;
	}

	return 0;
}

int fs_info(void)
{
	// Count the number of free spaces in FAT array
	int fatFreeSpaceCount = 0;
	for (int i = 0; i < superBlock->numDataBlocks; i++)
	{
		if (fatArray[i].next == FAT_FREE)
		{
			fatFreeSpaceCount++;
		}
	}

	// count the number of free root directories
	int freeRootDirectoryCount = 0;
	for (int i = 0; i < FS_FILE_MAX_COUNT; i++)
	{
		if (rootDirectory[i].sizeOfFile == 0)
		{
			freeRootDirectoryCount++;
		}
	}

	printf("FS Info:\n");
	printf("total_blk_count=%d\n", superBlock->numBlockVirtualDisk );
	printf("fat_blk_count=%d\n", superBlock->numBlocksFAT );
	printf("rdir_blk=%d\n", superBlock->numBlocksFAT+1);
	printf("data_blk=%d\n", superBlock->numBlocksFAT+2 );
	printf("data_blk_count=%d\n", superBlock->numDataBlocks);
	printf("fat_free_ratio=%d/%d\n", fatFreeSpaceCount, superBlock->numDataBlocks);
	printf("rdir_free_ratio=%d/%d\n", freeRootDirectoryCount, FS_FILE_MAX_COUNT);
	return 0;
}

int fs_create(const char *filename)
{	
	/*Error: No FS currently mounted*/
	if (!superBlock)
	{
		disk_error("No FS mounted");
		return -1;
	}
	/*Error: invalid filename*/
	int lengthOfFilename = strlen(filename);
	if (&filename[lengthOfFilename] != NULL){
		disk_error("Invalid filename");
		return -1;
	}
	/*Error: filename too long*/
	if (lengthOfFilename > FS_FILENAME_LEN){
		disk_error("Filename too long");
		return -1;
	}
	/*Error: file already exists*/
	if (!strcmp(rootDirectory->fileName, filename)){
		disk_error("File already exists");
		return -1;
	}
	
	/*Error: Root directory already contains the max amount of files*/
	// count the number of free root directories
	int freeRootDirectoryCount = 0;
	for (int i = 0; i < FS_FILE_MAX_COUNT; i++)
	{
		if (rootDirectory[i].sizeOfFile == 0)
		{
			freeRootDirectoryCount++;
		}
	}
	if(freeRootDirectoryCount == FS_FILE_MAX_COUNT)
	{
		disk_error("Max amount of files in root directory");
		return -1;
	}

	/*Go through the root directory and find the entry where everything is NULL*/
	for(int i = 0; i < FS_FILE_MAX_COUNT; i++)
	{
		//ASK TA: ASK if have nothing for filename, but has data ( size and index)
		if(lengthOfFilename == 0) //ASK TA: how to check file size and first index? 
		{
			strcpy(rootDirectory[i].fileName, filename);
			rootDirectory[i].sizeOfFile = 0;
			rootDirectory[i].firstIndex = FAT_EOC;	
		}
	}
	return 0;
}

/* Removing a file is the opposite procedure: the file’s entry must be emptied and all the data blocks 
containing the file’s contents must be freed in the FAT.*/


int fs_delete(const char *filename)
{
	/* TODO: Phase 2 */
	return 0;
}

int fs_ls(void)
{
	/*Error: No FS currently mounted*/
	if (!superBlock)
	{
		disk_error("No FS mounted");
		return -1;
	}
	
	int freeRootDirectoryCount = 0;
	for (int i = 0; i < FS_FILE_MAX_COUNT; i++)
	{
		if (rootDirectory[i].sizeOfFile == 0)
		{
			freeRootDirectoryCount++;
		}
	}
	int occupiedFileCount = FS_FILE_MAX_COUNT - freeRootDirectoryCount;

	printf("FS Ls:\n");
	for(int i = 0; i < occupiedFileCount; i++)
	{
		printf("file: %s, size: %d, data_blk: %d\n", 
		rootDirectory[i].fileName, rootDirectory[i].sizeOfFile, rootDirectory[i].firstIndex);
		/*file: test_fs.c, size: 11383, data_blk: 1*/
	}

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
