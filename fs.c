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
#define FD_MAX 32

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

struct __attribute__((__packed__)) fdTable
{
	char fileName[FS_FILENAME_LEN]; //(16 bytes) Filename
	int fd;
	int file_offset;
	int empty; // 0 default empty = 0, full = 1
	int open; // 0=close, 1 = open 
				
};

// creating objects of structs
struct superblock *superBlock;
struct FAT *fatArray;
struct rootdirectory *rootDirectory;
bool disk_open = false;
bool file_open = false;
struct fdTable *fdArray;

/* HELPER FUNCTIONS */

/*Checks if file is currently mounted */
bool checkIfFileOpen(struct superblock *superBlock)
{
	bool fileOpen = true;
	if (!superBlock)
	{
		disk_error("No FS mounted");
		fileOpen = false;
	}
	return fileOpen;
}

/*Checks if filename is invalid */
/*Invalid if:
	1. filename's total length exceeds the FS_FILENAME_LEN (including the NULL character)
	2. filename does not end with a NULL character
*/
bool checkFileNameValid(const char *filename)
{
	bool validFileName = true;
	int lengthOfFilename = strlen(filename);

	if (filename[lengthOfFilename] != '\0')
	{
		disk_error("Invalid filename");
		validFileName = false;
	}
	/*Error: filename too long*/
	if (lengthOfFilename > FS_FILENAME_LEN)
	{
		disk_error("Filename too long");
		validFileName = false;
	}
	return validFileName;
}

/*Checks if a file exists with the given filename*/
bool checkIfFileExists(const char *filename)
{
	bool fileExists = true;
	int count = 0;

	for (int i = 0; i < FS_FILE_MAX_COUNT; i++)
	{
		int lengthOfRootFile = strlen(rootDirectory[i].fileName);

		if ((lengthOfRootFile > 0) && (!strcmp(rootDirectory[i].fileName, filename)))
		{
			printf("Compared !!");
			count++;
		}
	}
	if (count == 0)
	{
		// disk_error("No file with that filename");
		fileExists = false;
	}
	return fileExists;
}

/*Checks if file descriptor is invalid*/
/*NEED TO STILL CHECK IF THIS WORKS; COULDN'T BECAUSE OF SEG FAULT MAYBE IN fs_open()*/
// bool checkFileDescriptorValid(int fd)
// {
// 	bool fdValid = true;
// 	if (fd > FD_MAX || fd < 0 || fdArray[fd].open == 0) 
	// {
	// 	//printf("File descriport invalid ")
	// 	disk_error("file descriptor is invalid");
	// 	fdValid = false;
	// }
// 	return fdValid;
// }

/*MAIN FUNCTIONS */

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
	
	fdArray = malloc(sizeof(struct fdTable)* FD_MAX);

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
	free(fdArray);

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
	printf("total_blk_count=%d\n", superBlock->numBlockVirtualDisk);
	printf("fat_blk_count=%d\n", superBlock->numBlocksFAT);
	printf("rdir_blk=%d\n", superBlock->numBlocksFAT + 1);
	printf("data_blk=%d\n", superBlock->numBlocksFAT + 2);
	printf("data_blk_count=%d\n", superBlock->numDataBlocks);
	printf("fat_free_ratio=%d/%d\n", fatFreeSpaceCount, superBlock->numDataBlocks);
	printf("rdir_free_ratio=%d/%d\n", freeRootDirectoryCount, FS_FILE_MAX_COUNT);
	return 0;
}

int fs_create(const char *filename)
{

	// /*Error: No FS currently mounted*/
	// // if(checkIfFileOpen(superBlock) == false)
	// // {
	// // 	return -1;
	// // }
	// printf("%d\n", checkIfFileOpen(superBlock));
	// if (!superBlock)
	// {
	// 	disk_error("No FS mounted");
	// 	return -1;
	// }
	// /*Error: invalid filename*/
	// int lengthOfFilename = strlen(filename);
	// if (filename[lengthOfFilename] != '\0')
	// {
	// 	disk_error("Invalid filename");
	// 	return -1;
	// }
	// /*Error: filename too long*/
	// if (lengthOfFilename > FS_FILENAME_LEN)
	// {
	// 	disk_error("Filename too long");
	// 	return -1;
	// }
	// printf("%d\n", checkFileNameValid(filename));
	// /*Error: Root directory already contains the max amount of files*/
	// // count the number of free root directories
	// int freeRootDirectoryCount = 0;
	// for (int i = 0; i < FS_FILE_MAX_COUNT; i++)
	// {
	// 	// int lenghtOfRootFile = strlen(rootDirectory[i].fileName);
	// 	if (rootDirectory[i].fileName[0] == '\0')
	// 	{
	// 		freeRootDirectoryCount++;
	// 	}
	// }
	// if (freeRootDirectoryCount == 0)
	// {
	// 	disk_error("Max amount of files in root directory");
	// 	return -1;
	// }

	// /* CHECK IF THIS WORKS */
	// /*Error: file already exists*/
	// for (int i = 0; i < FS_FILE_MAX_COUNT; i++)
	// {
	// 	// printf("Coming here in for loop\n");
	// 	if (!strcmp(rootDirectory[i].fileName, filename))
	// 	{
	// 		// disk_error();
	// 		return -1;
	// 	}
	// }

	/*Error Management: No FS currently mounted, Invalid file name,
		or file already exists*/
	if (checkIfFileOpen(superBlock) == 0 || checkFileNameValid(filename) == 0 || checkIfFileExists(filename) == 1)
	{
		return -1;
	}

	/*Error: Root directory already contains the max amount of files*/
	// count the number of free root directories
	int freeRootDirectoryCount = 0;
	for (int i = 0; i < FS_FILE_MAX_COUNT; i++)
	{
		// int lenghtOfRootFile = strlen(rootDirectory[i].fileName);
		if (rootDirectory[i].fileName[0] == '\0')
		{
			freeRootDirectoryCount++;
		}
	}
	if (freeRootDirectoryCount == 0)
	{
		disk_error("Max amount of files in root directory");
		return -1;
	}

	// ASK TA : OUTPUT
	/*
	mzubair@COE-CS-pc17:~/project3/apps$ ./test_fs.x add disk.fs test_fs.c
	occupiedFileCount = 2
	Coming here in for loop
	fs_create: File already exists
	thread_fs_add: Cannot create file
	mzubair@COE-CS-pc17:~/project3/apps$ ./fs_ref.x add disk.fs test_fs.c
	thread_fs_add: Cannot create file
	*/

	/*Go through the root directory and find the entry where everything is NULL*/
	for (int i = 0; i < FS_FILE_MAX_COUNT; i++)
	{

		// ASK TA: ASK if have nothing for filename, but has data (size and index)
		printf("inside for loop\n");
		// find empty spot in root directory
		if (rootDirectory[i].fileName[0] == '\0') // ASK TA: how to check file size and first index?
		{
			printf("inside if statement on the %dth iteration\n", i);
			strcpy(rootDirectory[i].fileName, filename);
			rootDirectory[i].sizeOfFile = 0;
			rootDirectory[i].firstIndex = FAT_EOC;

			return 0;
		}
	}
	return -1;
}

/* Removing a file is the opposite procedure: the file’s entry must be emptied and all the data blocks
containing the file’s contents must be freed in the FAT.*/
/**
 * fs_delete - Delete a file
 * @filename: File name
 *
 * Delete the file named @filename from the root directory of the mounted file
 * system.
 *
 * Return: -1 if no FS is currently mounted, or if @filename is invalid, or if
 * Return: -1 if @filename is invalid, if there is no file named @filename to
 * delete, or if file @filename is currently open. 0 otherwise.
 */

/*
	   Error checking:
	   1. First find the file with the file name
	   2. If there is a file existing in the root directory
		   to delete, then take that its first data block's index
			   to start the deleting process
	   3. Clearing the file from the fat array
		   1. From the first data block's index, access its content,  map on to the fat array
			   and set it to 0.
		   2. But before setting it to 0, take its content "next", and
			   use that info to get the next data block's entry
		   3. Do this until reached FAT_EOC as the entry
		   4. Then set that fat array's entry to 0
*/
int fs_delete(const char *filename)
{

	// /*Error: No FS is currently mounted*/
	// if (!superBlock)
	// {
	// 	disk_error("No FS mounted");
	// 	return -1;
	// }
	// /*Error: Filename is invalid*/
	// int lengthOfFilename = strlen(filename);
	// if (filename[lengthOfFilename] != '\0')
	// {
	// 	disk_error("Invalid filename");
	// 	return -1;
	// }
	// /*Error: No file exists with the @filename */
	// int count = 0;
	// for (int i = 0; i < FS_FILE_MAX_COUNT; i++)
	// {
	// 	int lengthOfRootFile = strlen(rootDirectory[i].fileName);
	// 	// printf("Coming here in for loop\n");
	// 	if ((lengthOfRootFile > 0) && (!strcmp(rootDirectory[i].fileName, filename)))
	// 	{
	// 		printf("Compared !!");
	// 		count++;
	// 	}
	// }
	// if (count == 0)
	// {
	// 	// disk_error("No file with that filename");
	// 	return -1;
	// }

	/*Error Management: No FS currently mounted, Invalid file name,
		or file does not exist*/
	if (checkIfFileOpen(superBlock) == 0 || checkFileNameValid(filename) == 0 || checkIfFileExists(filename) == 0)
	{
		return -1;
	}

	/*Error: file @filename is currently open */
	if (file_open == true)
	{
		disk_error("%s currently open\n", filename); // check for compilation
		return -1;
	}

	/*all the data blocks containing the file’s contents must be freed in the FAT.*/
	printf("Checkpoint 1\n");
	for (int i = 0; i < FS_FILE_MAX_COUNT; i++)
	{
		printf("entering  for\n");
		if ((strlen(rootDirectory[i].fileName) > 0) && (strcmp(rootDirectory[i].fileName, filename) == 0))
		{
			printf("Found the file with the file name\n");
			rootDirectory[i].fileName[0] = '\0';
			// rootDirectory[i].firstIndex = 2
			if (rootDirectory[i].sizeOfFile > 0)
			{
				uint16_t fatIndex = rootDirectory[i].firstIndex;			   // 2 index
				uint16_t nextFat = fatArray[rootDirectory[i].firstIndex].next; // 3 content
				while (nextFat != FAT_EOC)
				{
					printf("Entering while\n");
					fatIndex = nextFat; // 2-> 3 index
					nextFat = 0;		// 3-> 0 content
					nextFat = fatArray[fatIndex].next;
				} // end while

				nextFat = 0;
			} // end if
		}	  // end if
	}		  // end for

	return 0;
}

int fs_ls(void)
{
	/*Error: No FS currently mounted*/
	if (checkIfFileOpen(superBlock) == 0)
	{
		return -1;
	}

	printf("FS Ls:\n");
	for (int i = 0; i < FS_FILE_MAX_COUNT; i++)
	{
		/* Only prints out the root directories where it is not empty*/
		if (rootDirectory[i].fileName[0] != '\0')
		{
			printf("file: %s, size: %d, data_blk: %d\n",
				   rootDirectory[i].fileName, rootDirectory[i].sizeOfFile, rootDirectory[i].firstIndex);
		}
	}

	return 0;
}
/**
 * fs_open - Open a file
 * @filename: File name
 *
 * Open file named @filename for reading and writing, and return the
 * corresponding file descriptor. The file descriptor is a non-negative integer
 * that is used subsequently to access the contents of the file. The file offset
 * of the file descriptor is set to 0 initially (beginning of the file). If the
 * same file is opened multiple files, fs_open() must return distinct file
 * descriptors. A maximum of %FS_OPEN_MAX_COUNT files can be open
 * simultaneously.
 *
 * Return: -1 if no FS is currently mounted, or if @filename is invalid, or if
 * there is no file named @filename to open, or if there are already
 * %FS_OPEN_MAX_COUNT files currently open. Otherwise, return the file
 * descriptor.
 */






//setting empty = true for all fd's 
// void initializeEmpty(void ){
// 	for (int i=0; i<FD_MAX; i++){
// 		printf("ENTERING FOR LOOP");
// 		printf("%d\n", fdArray[i]->empty);
// 		fdArray[i]->empty = true;
// 	}
// }
// initializeEmpty();
	


int fs_open(const char *filename)
{
	// printf("In fs_open function\n");
	// /*Error: No FS currently mounted */
	// if (!superBlock)
	// {
	// 	disk_error("No FS mounted");
	// 	return -1;
	// }

	// /*Error: Filename is invalid */
	// int lengthOfFilename = strlen(filename);
	// if (filename[lengthOfFilename] != '\0')
	// {
	// 	disk_error("Invalid filename");
	// 	return -1;
	// }

	// /*Error: No file named @filename exists */
	// /* MAKE A HELPER FUNCTION*/
	// int count = 0;
	// for (int i = 0; i < FS_FILE_MAX_COUNT; i++)
	// {
	// 	int lengthOfRootFile = strlen(rootDirectory[i].fileName);
	// 	// printf("Coming here in for loop\n");
	// 	if ((lengthOfRootFile > 0) && (!strcmp(rootDirectory[i].fileName, filename)))
	// 	{
	// 		printf("Compared !!");
	// 		count++;
	// 	}
	// }
	// if (count == 0)
	// {
	// 	disk_error("No file with that filename");
	// 	return -1;
	// }
	
	// // int sizeOfFdArray = sizeof fdArray / sizeof fdArray[0];
	// // if fd array is empty , insert file to first index
	// printf("Checkpoint 1\n");

	/*Error Management: No FS currently mounted, Invalid file name,
		or file does not exist*/
	if (checkIfFileOpen(superBlock) == 0 || checkFileNameValid(filename) == 0 || checkIfFileExists(filename) == 0)
	{
		return -1;
	}

	int fdToReturn = -1;

	for (int i = 0; i < FD_MAX; i++)
	{
		printf("Checkpoint 2\n");
		printf("%d\n", fdArray[i].empty);
		printf("Checkpoint 3\n");
		if (fdArray[i].empty == 0)
		{
			printf("Checkpoint 4\n");
			strcpy(fdArray[i].fileName, filename);
			fdArray[i].fd = i;
			fdArray[i].file_offset = 0;
			fdArray[i].empty = 1;
			fdArray[i].open = 1;
			fdToReturn = fdArray[i].fd;
			file_open = true;

			return fdToReturn;
		}
	}
	if (fdToReturn == -1)
	{
		disk_error("No fd was found and allocated");
		return -1;
	}
	return -1;
	
}
/*
	1. Find first available file descriptor
		1. maybe have a flag that tells if the file descriptor is available or not
	2. When found:
		1. Set available flag to unavailable
		2. Set file offset to 0
		3. Set fd's filename to the filename input
*/

/**
 * fs_close - Close a file
 * @fd: File descriptor
 *
 * Close file descriptor @fd.
 *
 * Return: -1 if no FS is currently mounted, or if file descriptor @fd is
 * invalid (out of bounds or not currently open). 0 otherwise.
 */
int fs_close(int fd)
{
	/* TODO: Phase 3 */
	/*Error: No FS currently mounted */
	if (checkIfFileOpen(superBlock) == 0)
	{
		return -1;
	}

	/*Error: File descriptor is invalid */
	if (fd > FD_MAX || fd < 0 || fdArray[fd].open == 1)
	{
		// disk_error("file descriptor is invalid");
		return -1;
	}

	/*Close the file descriptor @fd*/
	/*
		1. Reset file name to NULL (?)
		2. Set file descriptor to -1
		3. Set file_offset back to 0
		4. Set the index in fdArray to being empty (aka empty == true)
	*/
	strcpy(fdArray[fd].fileName, "");
	fdArray[fd].fd = -1;
	fdArray[fd].file_offset = 0;
	fdArray[fd].empty = true;
	file_open = false;

	return 0;
}

/**
 * fs_stat - Get file status
 * @fd: File descriptor
 *
 * Get the current size of the file pointed by file descriptor @fd.
 *
 * Return: -1 if no FS is currently mounted, of if file descriptor @fd is
 * invalid (out of bounds or not currently open). Otherwise return the current
 * size of file.
 */
int fs_stat(int fd)
{
	/* TODO: Phase 3 */
	/*Error: No FS currently mounted */
	if (checkIfFileOpen(superBlock) == 0)
	{
		return -1;
	}
	/*Error: File descriptor is invalid */
	if (fd > FD_MAX || fd < 0 || fdArray[fd].open == 0)
	{
		// disk_error("file descriptor is invalid");
		return -1;
	}

	/*
		1. From the file descriptor, find filename
		2. From filename find the current size of the file
	*/

	int fdFileSize = -1;
	for (int i = 0; i < FS_FILE_MAX_COUNT; i++)
	{
		if (fdArray[fd].fileName == rootDirectory[i].fileName)
		{
			fdFileSize = rootDirectory[i].sizeOfFile;
			break;
		}
	}
	if (fdFileSize == -1)
	{
		disk_error("file does not exist");
		return -1;
	}
	return fdFileSize;
}

/**
 * fs_lseek - Set file offset
 * @fd: File descriptor
 * @offset: File offset
 *
 * Set the file offset (used for read and write operations) associated with file
 * descriptor @fd to the argument @offset. To append to a file, one can call
 * fs_lseek(fd, fs_stat(fd));
 *
 * Return: -1 if no FS is currently mounted, or if file descriptor @fd is
 * invalid (i.e., out of bounds, or not currently open), or if @offset is larger
 * than the current file size. 0 otherwise.
 */
int fs_lseek(int fd, size_t offset)
{
	/* TODO: Phase 3 */
	/*Error: No FS currently mounted */
	if (checkIfFileOpen(superBlock) == 0)
	{
		return -1;
	}
	/*Error: File descriptor is invalid */
	if (fd > FD_MAX || fd < 0 || fdArray[fd].open == 0)
	{
		// disk_error("file descriptor is invalid");
		return -1;
	}
	/*Error: Offset is larger than the current file size*/
	if (offset > fs_stat(fd))
	{
		// disk_error("@offset is larger than the current file size");
		return -1;
	}
	/*
		1. find the file associated with the file descriptor
		2. move file's offset to the @offset
	*/
	fdArray[fd].file_offset = offset;

	return 0;
}

int fs_write(int fd, void *buf, size_t count)
{
	/* TODO: Phase 4 */
	return 0;
}
/*
For these functions, you will probably need a few helper functions.
For example, you will need a function that returns the index of the 
data block corresponding to the file’s offset

When reading or writing a certain number of bytes from/to a file, 
you will also need to deal properly with possible “mismatches” between
the file’s current offset, the amount of bytes to read/write, the size
of blocks, etc.

For example, let’s assume a reading operation for which the file’s offset 
is not aligned to the beginning of the block or the amount of bytes to read
doesn’t span the whole block. You will probably need to read the entire block
into a bounce buffer first, and then copy only the right amount of bytes from
the bounce buffer into the user-supplied buffer.
*/

/**
 * fs_read - Read from a file
 * @fd: File descriptor
 * @buf: Data buffer to be filled with data
 * @count: Number of bytes of data to be read
 *
 * Attempt to read @count bytes of data from the file referenced by file
 * descriptor @fd into buffer pointer by @buf. It is assumed that @buf is large
 * enough to hold at least @count bytes.
 *
 * The number of bytes read can be smaller than @count if there are less than
 * @count bytes until the end of the file (it can even be 0 if the file offset
 * is at the end of the file). The file offset of the file descriptor is
 * implicitly incremented by the number of bytes that were actually read.
 *
 * Return: -1 if no FS is currently mounted, or if file descriptor @fd is
 * invalid (out of bounds or not currently open), or if @buf is NULL. Otherwise
 * return the number of bytes actually read.
 */


int fs_read(int fd, void *buf, size_t count)
{
	/* TODO: Phase 4 */
	//Error:  no FS is currently mounted,
	if (checkIfFileOpen(superBlock) == 0)
	{
		return -1;
	}
	/*Error: File descriptor is invalid */
	if (fd > FD_MAX || fd < 0 || fdArray[fd].open == 0)
	{
		// disk_error("file descriptor is invalid");
		return -1;
	}
	//Error: if buf is NULL
	if ( buf == NULL){
		disk_error("Buf is NULL");
		return -1;
	}

	//start reading
	/* 3 cases:
	(1) Small operations -> Less than a block
	(2) First/last block on big operations
	Access entire blocks except first or last
	block of operation
	Partial access on first or last block
	(3) Full block
	*/
	//bounce buffer 
	char bounce_buf[BLOCK_SIZE]
	int offset = fdArray[fd].offset;

	int currBlockNum = offset / BLOCK_SIZE; //50/16 = 3
	int bounceBufOffset = offset % BLOCK_SIZE //50%16
	

	return 0;
}
