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

struct __attribute__((__packed__)) superblock
{
	char signature[8];			  //(8 characters) signature
	uint16_t numBlockVirtualDisk; //(2 bytes) total amount of blocks of virtual disk
	uint16_t rootBlockIndex;	  //(2 bytes) root directory block index
	uint16_t dataBlockStartIndex; //(2 bytes) data block start index
	uint16_t numDataBlocks;		  // (2 bytes)  amount of data blocks
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
	int open;  // 0=close, 1 = open
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
			// printf("Compared !!");
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
bool checkFileDescriptorValid(int fd)
{
	bool fdValid = true;
	if (fd > FD_MAX || fd < 0 || fdArray[fd].open == 0)
	{
		disk_error("file descriptor is invalid");
		fdValid = false;
	}
	return fdValid;
}

/*Finds the next empty FAT block*/
int emptyFATIndex(void)
{
	int nextEmptyFAT = 0;
	// printf("superBlock->numBlocksFat: %d\n", superBlock->numBlocksFAT);
	// printf("superBlock->numofDataBlocks: %d\n", superBlock->numDataBlocks);
	for (int i = 1; i < superBlock->numDataBlocks; i++)
	{
		printf("fattArray[%d].next: %d\n", i, fatArray[i].next);
		if (fatArray[i].next == 0)
		{
			nextEmptyFAT = i;
			printf("empty in loop: %d\n", nextEmptyFAT);
			break;
		}
	}
	return nextEmptyFAT;
}

/*Finds the total number of empty FAT blocks*/
int totalEmptyFATBlocks(void)
{
	int totalEmptyFAT = 0;
	for (int i = 0; i < superBlock->numDataBlocks; i++)
	{
		if (fatArray[i].next == 0)
		{
			totalEmptyFAT++;
		}
	}
	return totalEmptyFAT;
}

/*Finds the current FAT block index*/
int findCurFatBlockIndex(int fileLocation, int curBlockNum)
{
	int curFatBlockIndex;
	curFatBlockIndex = rootDirectory[fileLocation].firstIndex;
	for (int i = 0; i < curBlockNum; i++)
	{
		if (curFatBlockIndex == FAT_EOC)
		{
			return -1;
		}
		curFatBlockIndex = fatArray[curFatBlockIndex].next;
	}
	return curFatBlockIndex;
}

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
		if (block_read(i + 1, (void *)fatArray + (i * BLOCK_SIZE)) == -1) // ask TA about
		{
			disk_error("Error in reading");
			return -1;
		}
	}

	// rootDirectory initialization
	rootDirectory = malloc(sizeof(struct rootdirectory) * FS_FILE_MAX_COUNT);
	if (block_read(superBlock->numBlocksFAT + 1, (void *)rootDirectory) == -1) // ask TA about
	{
		disk_error("Cannot read from root directory block");
		return -1;
	}

	fdArray = malloc(sizeof(struct fdTable) * FD_MAX);

	return 0;
}

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
		// printf("inside for loop\n");
		// find empty spot in root directory
		if (rootDirectory[i].fileName[0] == '\0') // ASK TA: how to check file size and first index?
		{
			// printf("inside if statement on the %dth iteration\n", i);
			strcpy(rootDirectory[i].fileName, filename);
			rootDirectory[i].sizeOfFile = 0;
			rootDirectory[i].firstIndex = FAT_EOC;

			return 0;
		}
	}
	return -1;
}

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
	// printf("Checkpoint 1\n");
	for (int i = 0; i < FS_FILE_MAX_COUNT; i++)
	{
		// printf("entering  for\n");
		if ((strlen(rootDirectory[i].fileName) > 0) && (strcmp(rootDirectory[i].fileName, filename) == 0))
		{
			// printf("Found the file with the file name\n");
			rootDirectory[i].fileName[0] = '\0';
			// rootDirectory[i].firstIndex = 2
			if (rootDirectory[i].sizeOfFile > 0)
			{
				// uint16_t fatIndex = rootDirectory[i].firstIndex;			   // 2 index
				// uint16_t nextFat = fatArray[rootDirectory[i].firstIndex].next; // 3 content
				int fatIndex = rootDirectory[i].firstIndex; // 2 index
				int nextFat;								// 3 content
				printf("Checking FAT array: %d\n", emptyFATIndex());
				while (fatIndex != FAT_EOC)
				{
					// printf("Entering while\n");
					// fatIndex = nextFat; // 2-> 3 index
					// nextFat = 0;		// 3-> 0 content
					// nextFat = fatIndex;
					nextFat = fatArray[fatIndex].next;
					fatArray[fatIndex].next = 0;
					fatIndex = nextFat;

				} // end while
				rootDirectory[i].sizeOfFile = 0;
				// nextFat = 0;
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

int fs_open(const char *filename)
{

	/*Error Management: No FS currently mounted, Invalid file name,
		or file does not exist*/
	if (checkIfFileOpen(superBlock) == 0 || checkFileNameValid(filename) == 0 || checkIfFileExists(filename) == 0)
	{
		return -1;
	}

	int fdToReturn = -1;

	for (int i = 0; i < FD_MAX; i++)
	{
		// printf("Checkpoint 2\n");
		// printf("%d\n", fdArray[i].empty);
		// printf("Checkpoint 3\n");
		if (fdArray[i].empty == 0)
		{
			// printf("Checkpoint 4\n");
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

int fs_close(int fd)
{
	/*Error: No FS currently mounted or file descriptor is invalid*/
	if (checkIfFileOpen(superBlock) == 0 || checkFileDescriptorValid(fd) == 0)
	{
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
	fdArray[fd].open = 0;

	return 0;
}

int fs_stat(int fd)
{
	/*Error: No FS currently mounted or file descriptor is invalid*/
	if (checkIfFileOpen(superBlock) == 0 || checkFileDescriptorValid(fd) == 0)
	{
		return -1;
	}

	/*
		1. From the file descriptor, find filename
		2. From filename find the current size of the file
	*/

	int fdFileSize = -1;
	// printf("Filename is %s\n", fdArray[fd].fileName);
	for (int i = 0; i < FS_FILE_MAX_COUNT; i++)
	{
		// printf("Root Directory's file name is %s\n", rootDirectory[i].fileName);
		if (!strcmp(fdArray[fd].fileName, rootDirectory[i].fileName))
		{
			// printf("inside if for stat\n");
			fdFileSize = rootDirectory[i].sizeOfFile;
			// printf("fdFileSize: %d\n", fdFileSize);
			// printf("File offset: %d\n", fdArray[fd].file_offset);
			break;
		}
	}
	if (fdFileSize == -1)
	{
		disk_error("file does not exist");
		return -1;
	}
	// printf("\n");
	return fdFileSize;
}

int fs_lseek(int fd, size_t offset)
{

	/*Error: No FS currently mounted, file descriptor is invalid,
		or offset is larger than current file size*/
	if (checkIfFileOpen(superBlock) == 0 || checkFileDescriptorValid(fd) == 0 || offset > fs_stat(fd))
	{
		return -1;
	}

	/*
		1. find the file associated with the file descriptor
		2. move file's offset to the @offset
	*/
	fdArray[fd].file_offset = offset;

	return 0;
}

/**
 * fs_write - Write to a file
 * @fd: File descriptor
 * @buf: Data buffer to write in the file
 * @count: Number of bytes of data to be written
 *
 * Attempt to write @count bytes of data from buffer pointer by @buf into the
 * file referenced by file descriptor @fd. It is assumed that @buf holds at
 * least @count bytes.
 *
 * When the function attempts to write past the end of the file, the file is
 * automatically extended to hold the additional bytes. If the underlying disk
 * runs out of space while performing a write operation, fs_write() should write
 * as many bytes as possible. The number of written bytes can therefore be
 * smaller than @count (it can even be 0 if there is no more space on disk).
 *
 * Return: -1 if no FS is currently mounted, or if file descriptor @fd is
 * invalid (out of bounds or not currently open), or if @buf is NULL. Otherwise
 * return the number of bytes actually written.
 */

/*
	Steps:
	1. Error checking
	2. Using the file descriptor @fd,
		A. find its location in the root directory to get its file size
			 and index of the first data block
		B. Find the file offset
	3. Find/Create:
		A. How many blocks are needed to write the data to the file?
			1. Need to figure out the part of how to extend the file to hold additional bytes***
		B. curBlockNum
		C. curFatBlockIndex
		D. bounce buffer
		E. numOfBytesWritten (needs to be returned at the end of the function)
		F. bounceBufOffset
	4.  Find available blocks from the beginnning of the FAT
		following the "first-fit" strategy and store the information of which blocks are available
		and how many there are.
	5. Figure out how many data blocks are available on the disk to stop writing when there is no more space left
	6. Write to the file block per block using block_write()
	7. Change the FAT entries that points to the correct next data block accordingly
	8. Change the file's size to current file size and the number of bytes written
	9. Return the number of bytes written.

	db1 , db2. file offste = 10, in db1, write 20 bytes
	4096 4096
	full  4000
	10+20
	abcdefghsdajds, offset 2, zebra
	abzebrahsdajds
	zebra, offset 3(b), apple
	zebap, ple


*/

int fs_write(int fd, void *buf, size_t count)
{
	// /* TODO: Phase 4 */
	// /*Error: No FS currently mounted, file descriptor is invalid,
	// 	or @buf is NULL*/
	if (checkIfFileOpen(superBlock) == 0 || checkFileDescriptorValid(fd) == 0 || buf == NULL)
	{
		return -1;
	}

	// /*Find the location of the file in the root directory to access its attributes */
	int fileLocation = 0;
	for (int i = 0; i < FS_FILE_MAX_COUNT; i++)
	{
		if (strncmp(fdArray[fd].fileName, rootDirectory[i].fileName, FS_FILENAME_LEN) == 0)
		{
			fileLocation = i;
		}
	}
	/*Buffer to temporarily hold data that is going to be written to the file*/
	char bounceBuf[BLOCK_SIZE];
	int bounceBufOffSet = fdArray[fd].file_offset % BLOCK_SIZE; // 4080 % 4096 =
	int currBlockNum = fdArray[fd].file_offset / BLOCK_SIZE;	// 50 / 16 = 3
	printf("current Block number: %d\n", currBlockNum);

	/*C. curFatBlockIndex*/
	int currentFATBlockIndex = findCurFatBlockIndex(fileLocation, currBlockNum);
	printf("currentFatBlockIndex: %d\n", currentFATBlockIndex);

	/*Buffer from where we are getting the data to write to the file*/
	char *writeBuf = (char *)buf;

	/*Find size of the file and the file's offset with @fd*/
	int fileSize = rootDirectory[fileLocation].sizeOfFile;
	int fileOffset = fdArray[fd].file_offset;
	int freeSpaceOnBlock = BLOCK_SIZE - fileSize;
	printf("Size of File: %d\n", fileSize);
	printf("File's offset: %d\n", fileOffset);
	printf("Free space on block: %d\n", freeSpaceOnBlock);

	/*
		1. totalBytesWritten = actually the number of bytes written to the file
		2. numOfBlocksToWrite = number of blocks needed to write from the buffer to the file
			1. Depends on the amount of data that needs to be written (@count)
				1. Each block's size is 4096, so figure out how many blocks needed comparing to the @count DONE
			2. Check if there are available data blocks comparing to how many are needed
				1. For this figure out how many available data blocks are left for the file
				2. If there are no available data blocks left and NEED to extend file by increasing FAT blocks
			3. Check how many free FAT blocks are there, to check how much space is left on the disk (# of free FAT blocks)
				1. then the numOfBlockToWrite will be either be how many free FAT blocks that will be used
				2. or numOfBlocksToWrite will be all the free FAT blocks and however much space is left on the disk
		3. bytesToWrite = number of bytes to write to the current block
		4. bytesLeftToWrite = the number of bytes left to write from @count after writing to the file
	*/
	int totalBytesWritten = 0;
	int numOfBlocksToWrite = (count / BLOCK_SIZE) + 1; // new data blocks to allocate
	int bytesToWrite = 0;
	int bytesLeftToWrite = count;
	int nextEmptyFatIndex = emptyFATIndex();
	int spaceOnDisk = totalEmptyFATBlocks(); // total empty FAT blocks
	int firstFileOffSet = fileOffset;
	printf("empty FAT index: %d\n", nextEmptyFatIndex);
	printf("Total number of empty FAT indices: %d\n", spaceOnDisk);
	printf("count: %d\n", (int)count);
	printf("numOfBlocksToWrite: %d\n", numOfBlocksToWrite);
	printf("\n");

	/*Step to take when there is no more space left on the file, so need to extend it*/
	if (currentFATBlockIndex == FAT_EOC)
	{
		/*set the current FAT block index to the next empty FAT index*/
		currentFATBlockIndex = nextEmptyFatIndex;
		rootDirectory[fileLocation].firstIndex = nextEmptyFatIndex;
	}

	/*Makes sure that the number of blocks to write is in boundary of space on the disk*/
	if (numOfBlocksToWrite > spaceOnDisk)
	{
		numOfBlocksToWrite = spaceOnDisk;
	}
	printf("numOfBlocksToWrite: %d\n", numOfBlocksToWrite);

	/*Write to file block by block*/
	for (int i = 0; i < numOfBlocksToWrite; i++)
	{
		printf("i: %d\n", i);
		/*file's offset is not aligned to the beginning of the block*/
		if ((fileOffset > 0) && (bytesLeftToWrite == count))
		{
			printf("(fileOffset > 0) && (bytesLeftToWrite == count)\n---------------\n");
			bytesToWrite = freeSpaceOnBlock;
			block_read(currentFATBlockIndex, bounceBuf);
			memcpy(bounceBuf + bounceBufOffSet, writeBuf, bytesToWrite);
			block_write(currentFATBlockIndex + superBlock->dataBlockStartIndex, bounceBuf);

			bytesLeftToWrite -= bytesToWrite;
			totalBytesWritten += bytesToWrite;
			fileOffset += bytesToWrite;
			printf("File's offset now: %d\n", fileOffset);
			printf("Bytes left to write: %d\n", bytesLeftToWrite);
			printf("Total bytes written so far: %d\n", totalBytesWritten);

			/*Set the current FAT block's next pointer to the next empty FAT block, if there is more bytes left to write*/
			if (bytesLeftToWrite != 0)
			{
				// fatArray[currentFATBlockIndex].next = 1;
				nextEmptyFatIndex = emptyFATIndex();
				fatArray[currentFATBlockIndex].next = nextEmptyFatIndex;
				printf("Checking FAT array: %d\n", emptyFATIndex());
				currentFATBlockIndex = fatArray[currentFATBlockIndex].next;
			}
			else /*Sets current FAT block's next pointer to EOC*/
			{
				nextEmptyFatIndex = FAT_EOC;
				fatArray[currentFATBlockIndex].next = nextEmptyFatIndex;
				printf("Checking FAT array: %d\n", emptyFATIndex());
			}
			break;
		}

		/*In case of the need to write partially to the block, and not in entirety*/
		if (bytesLeftToWrite < BLOCK_SIZE)
		{
			printf("bytesLeftToWrite < BLOCK_SIZE\n---------------\n");
			memcpy(bounceBuf, writeBuf, bytesLeftToWrite);
			block_write(currentFATBlockIndex + superBlock->dataBlockStartIndex, bounceBuf);

			totalBytesWritten += bytesLeftToWrite;
			fileOffset += bytesLeftToWrite;
			bytesLeftToWrite -= bytesLeftToWrite;
			printf("File's offset now: %d\n", fileOffset);
			printf("Bytes left to write: %d\n", bytesLeftToWrite);
			printf("Total bytes written so far: %d\n", totalBytesWritten);

			nextEmptyFatIndex = FAT_EOC;
			fatArray[currentFATBlockIndex].next = nextEmptyFatIndex;
			printf("Checking FAT array: %d\n", emptyFATIndex());
			break;
		}

		printf("In none of the if statements\n---------------\n");
		/*files's offset is aligned perfectly to the beginning of the block
		 AND bytesLeftToWrite is greater than or equal to BLOCK_SIZE*/
		memcpy(bounceBuf, writeBuf, BLOCK_SIZE);
		block_write(currentFATBlockIndex + superBlock->dataBlockStartIndex, bounceBuf);

		bytesLeftToWrite -= BLOCK_SIZE;
		totalBytesWritten += BLOCK_SIZE;
		fileOffset += BLOCK_SIZE;
		printf("File's offset now: %d\n", fileOffset);
		printf("Bytes left to write: %d\n", bytesLeftToWrite);
		printf("Total bytes written so far: %d\n", totalBytesWritten);

		/*Set the current FAT block's next pointer to the next empty FAT block, if there is more bytes left to write*/
		if (bytesLeftToWrite != 0)
		{
			fatArray[currentFATBlockIndex].next = 1;
			nextEmptyFatIndex = emptyFATIndex();
			fatArray[currentFATBlockIndex].next = nextEmptyFatIndex;
			printf("Checking FAT array: %d\n", emptyFATIndex());
			currentFATBlockIndex = fatArray[currentFATBlockIndex].next;
		}
		else /*Sets current FAT block's next pointer to EOC*/
		{
			nextEmptyFatIndex = FAT_EOC;
			fatArray[currentFATBlockIndex].next = nextEmptyFatIndex;
			printf("Checking FAT array: %d\n", emptyFATIndex());
		}
	}
	currentFATBlockIndex = fatArray[currentFATBlockIndex].next;
	rootDirectory[fileLocation].sizeOfFile = firstFileOffSet + totalBytesWritten;

	return totalBytesWritten;
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

// start reading
/* 3 cases:
(1) Small operations -> Less than a block
(2) First/last block on big operations
Access entire blocks except first or last
block of operation
Partial access on first or last block
(3) Full block
*/
/*
	1. If countOfBytesToRead is greater than the BLOCK_SIZE, then definitely
		need more than one blocks to be read to the buffer
		A. We need to track how many blocks we need for the amount of data
			we are reading to the buffer
		B. Then for each block, figure how many bytes we are reading to the buffer
	2. If countOfBytesToRead is less than the BLOCK_SIZE, then
		use a bounce buffer to first read the entire data block to
		the bounce buffer and then memcpy() the needed amount of data to
		the user buffer @buf.
*/
// read whole block

// if count > block_size
//		bytesToRead = (10-4096) = 4086 - from block 1
//		increment currFatBlockINdex ++
//		bytesLeftToRead = count - bytesToread = 5000-4086 = 914 -> bytes from next block
//		bytesToRead = 4086+914 == count - done
// If count < block_size //_> offset = 10, count = 20, block_size = 4096
//		-> use bounce buffer
//		->bounceoffset = 10%4096 -> 10 -> read 20 bytes
//		newOffset = offset +count

/*
	1. Using the file descriptor @fd, find its location in the root directory to
		get its file size and index of the first data block
	2. Check actually what is the number of bytes to be read because of the file offset position
		a. Smaller than @count
		b. Exactly @count
	3. Create bounce buffer to temporarily store data from the entire data block
	4. Find in the fat array, the current block index, to map out the data block to get data from
	5. Intialize a variable for the offset in bounce buffer where to start reading data from
	6. Read block by block into the bounce buffer through block_read()
	7. memcpy() from bounce buffer to user buffer @buf, the amount of data needed
	8. Add to the fd's offset in the fdArray the number of bytes actually read
	9. Return the number of bytes actually read
*/
int fs_read(int fd, void *buf, size_t count)
{
	/*Error: No FS currently mounted, file descriptor is invalid,
		or @buf is NULL*/
	if (checkIfFileOpen(superBlock) == 0 || checkFileDescriptorValid(fd) == 0 || buf == NULL)
	{
		return -1;
	}
	// printf("In read function\n");
	/*Find the location of the file in the root directory to access its attributes */
	int fileLocation = 0;
	for (int i = 0; i < FS_FILE_MAX_COUNT; i++)
	{
		if (strncmp(fdArray[fd].fileName, rootDirectory[i].fileName, FS_FILENAME_LEN) == 0)
		{
			fileLocation = i;
		}
	}
	// printf("fileLocation: %d\n", fileLocation);

	/*Check actually what is the number of bytes to be read because of the file offset position
		a. Smaller than @count
		b. Exactly @count
	*/
	int countOfBytesToRead = 0;
	if (fdArray[fd].file_offset + count > rootDirectory[fileLocation].sizeOfFile)
	{
		countOfBytesToRead = abs(rootDirectory[fileLocation].sizeOfFile - fdArray[fd].file_offset);
	}
	else
	{
		countOfBytesToRead = count;
	}

	// printf("Read CP1\n");
	/*Create bounce buffer*/
	char bounceBuf[BLOCK_SIZE];
	char *readBuf = (char *)buf;
	int bounceBufOffSet = fdArray[fd].file_offset % BLOCK_SIZE; // 50 % 16 = 2
	int currBlockNum = fdArray[fd].file_offset / BLOCK_SIZE;	// 50 / 16 = 3
	// 16 size block, offset 50, read 20 bytes
	// 0-16, 17-32, 33-48, 49-64, 65-80
	// 0,     1,     2     3       4
	//                     currBlockNUm(14 bytes), 6 bytes left-> move to next block = 4, read bytes 65-70
	//						49 50
	//						bounceOffset = 0, bytesToRead = if (20-16>=0),  copy the whole block (20-16>=0)
	//						bytes left tp read = 20-16 = 4-> move to next block 4 < 16 -> bounce buffer

	/*Find in the fat array, the current block index, to map out the data block to get data from*/
	/*
		1. Start from the first index of the data block
		2. Keep on going to the next data block until reached the current block number
		3. Set the currentFATBlockIndex to current data block index
	*/
	/*FUNCTIONS TO USE:
	void *memcpy(void *dest, const void * src, size_t n) //size_t n = strlen(src)+ 1
	int block_read(size_t block, void *buf);
	*/
	// printf("Read CP2\n");
	int currentFATBlockIndex = rootDirectory[fileLocation].firstIndex;
	// printf("currBlockNum: %d\n", currBlockNum);
	// printf("currentFATBlockIndex @671 aka rootDirectory->firstIndex: %d\n", currentFATBlockIndex);
	for (int i = 0; i < currBlockNum; i++) // change it to <=??
	{
		// printf("currentFATBlockIndex setting loop\n");
		if (currentFATBlockIndex == FAT_EOC)
		{
			return -1;
		}
		currentFATBlockIndex = fatArray[currentFATBlockIndex].next;
	}
	// printf("currentFATBlockIndex @681: %d\n", currentFATBlockIndex);

	/*Variable to store the number of bytes actually read*/
	int numBytesRead = 0;
	int numOfBlocksToRead = (countOfBytesToRead / BLOCK_SIZE) + 1;
	// printf("countOfBytesToRead: %d\n", countOfBytesToRead);
	// printf("numOfBlocksToRead = %d\n", numOfBlocksToRead);
	// printf("numBytesRead at start = %d\n", numBytesRead);
	// printf("\n");

	// printf("Read CP3\n");
	if (countOfBytesToRead == BLOCK_SIZE) // IDEAL CASE
	{
		printf("Read CP4: FIRST IF STATEMENT\n");
		for (int i = 1; i <= numOfBlocksToRead; i++)
		{
			block_read(currentFATBlockIndex, readBuf);
			numOfBlocksToRead += 1;
			fdArray[fd].file_offset += BLOCK_SIZE;
			numBytesRead += BLOCK_SIZE;
			currentFATBlockIndex = fatArray[currentFATBlockIndex].next;
		}
		// printf("Read CP5\n");
		// copy the whole block to bounce buff
	}
	else if (countOfBytesToRead < BLOCK_SIZE) // single block
	{
		// need bounce buffer
		printf("Read CP6: SECOND IF STATEMENT\n");
		// printf("currentFATBlockIndex: %d\n", currentFATBlockIndex);
		// printf("File Offset before reading: %d\n", fdArray[fd].file_offset);
		// block_read(currentFATBlockIndex + superBlock->dataBlockStartIndex, bounceBuf);
		printf("CP1\n");
		printf("Current FAT: %d\n", currentFATBlockIndex);
		printf("Current FAT + super: %d\n", currentFATBlockIndex + superBlock->dataBlockStartIndex);
		block_read(currentFATBlockIndex + superBlock->dataBlockStartIndex, bounceBuf);
		memcpy(readBuf, bounceBuf, countOfBytesToRead);
		numBytesRead += countOfBytesToRead;
		fdArray[fd].file_offset += countOfBytesToRead;
		// printf("File Offset after reading: %d\n", fdArray[fd].file_offset);
		// printf("numBytesRead at end = %d\n", numBytesRead);
	}
	else if (countOfBytesToRead > BLOCK_SIZE) // countOfBytesToRead > BLOCK_SIZE //multiple blocks
	{
		int firstLoopEntered = 0;
		// offset = 0, count = 5000, block_size = 4096
		// 5000/4096 = 1.22 = 1 => 1+1 = 2
		// Curr bLock = 0
		printf("Read CP7: THIRD IF STATEMENT\n");
		if (fdArray[fd].file_offset == 0) // All blocks are read in entirety except for the last one = full
		{
			firstLoopEntered = 1;
			printf("Read CP8\n");
			for (int i = 1; i <= numOfBlocksToRead; i++)
			// while ( currentFATBlockIndex != 65535)
			{
				if (countOfBytesToRead < BLOCK_SIZE) // last block not full
				{
					printf("Read CP10\n");
					char *endBuff[countOfBytesToRead - 1];
					// printf("COUNT < BLOCK SIZE\n");
					// printf("currFATIndex: %d\n", currentFATBlockIndex);
					// printf("File offset: %d\n", fdArray[fd].file_offset);
					// use bounce buffer
					block_read(currentFATBlockIndex, endBuff);
					// need to read bytes from 4096 - 5000 = 904
					memcpy(readBuf, endBuff, countOfBytesToRead);
					numBytesRead += countOfBytesToRead;
					fdArray[fd].file_offset += countOfBytesToRead;
					break;
				}

				// printf(" \n");
				// printf("File Offset before reading: %d\n", fdArray[fd].file_offset);
				// read from first block
				block_read(currentFATBlockIndex, readBuf);
				numBytesRead += BLOCK_SIZE;
				// printf("numBytesRead so far: %d\n", numBytesRead);
				fdArray[fd].file_offset += BLOCK_SIZE;
				// printf("File Offset after reading: %d\n", fdArray[fd].file_offset);

				countOfBytesToRead = abs(countOfBytesToRead - BLOCK_SIZE); // 9000-4096 = 904
				// printf("Count of bytes to read: %d\n", countOfBytesToRead);
				// move to the next data block
				// printf("currFATIndex: %d\n", currentFATBlockIndex);
				currentFATBlockIndex = fatArray[currentFATBlockIndex].next; // block 2
																			// printf("currFATIndex: %d\n", currentFATBlockIndex);
																			// numOfBlocksToRead +=1 ;
			}
		}
		/*  9000 0-4095, 4096-8191-8192-12287
		Block = 1 : offset = 0
		BytesToRead= 4096
		offset = 0+ 4096 = 4096
		CountbytesToread = 9000-4096 = 4904
		block=2
		Block =2: offset=4096 ,
		BytesToRaed = 8192
		offset = 4096+4096 = 8192
		countOfBytesToRead = 4904-4096 = 808
		block = 3
		Block 3: offset = 8192
		biunce = 3
		*/

		if (fdArray[fd].file_offset > 0 && firstLoopEntered == 0) // first Block not full , all other blocks full
		{														  // offset 10, count = 5000,
			printf("Read CP9\n");
			// first block not full, use bounce buffer
			block_read(currentFATBlockIndex, bounceBuf); // block = 0
			numBytesRead = BLOCK_SIZE - bounceBufOffSet; // 4096 - 10 = 4086
			memcpy(readBuf, bounceBuf, numBytesRead);
			// increment block
			currentFATBlockIndex = fatArray[currentFATBlockIndex].next;
			numOfBlocksToRead += 1;
			// increment offset
			fdArray[fd].file_offset += numBytesRead;				// 10+4086 = 4098
			countOfBytesToRead = countOfBytesToRead - numBytesRead; // 5000-4096 = 914
			for (int i = 2; i <= numOfBlocksToRead; i++)
			{ // last block full , offset = 10, count = 8182
				// continue reading next blocks, full
				block_read(currentFATBlockIndex, readBuf);
				numBytesRead += BLOCK_SIZE;
				currentFATBlockIndex = fatArray[currentFATBlockIndex].next;
				numOfBlocksToRead += 1;
				fdArray[fd].file_offset += BLOCK_SIZE;
				countOfBytesToRead = countOfBytesToRead - numBytesRead;

				if (countOfBytesToRead < BLOCK_SIZE) // last block not full
				{
					// use bounce buffer
					block_read(currentFATBlockIndex, bounceBuf);
					// need to read bytes from 4096 - 5000 = 904
					memcpy(readBuf, bounceBuf, countOfBytesToRead);
					numBytesRead += countOfBytesToRead;
					fdArray[fd].file_offset += countOfBytesToRead;
				} // end if
			}	  // end for
		}
	}

	return numBytesRead;
}
