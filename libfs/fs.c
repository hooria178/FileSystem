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

struct __attribute__((__packed__)) superblock
{
    char signature[8];            //(8 characters) signature
    uint16_t numBlockVirtualDisk; //(2 bytes) total amount of blocks of virtual disk
    uint16_t rootBlockIndex;      //(2 bytes) root directory block index
    uint16_t dataBlockStartIndex; //(2 bytes) data block start index
    uint16_t numDataBlocks;       // (2 bytes)  amount of data blocks
    uint8_t numBlocksFAT;         //(1 bytes)// number of blocks for FAT
    uint8_t padding[4079];        //(4079 bytes)// unsused/padding  ASK THIS
};

struct __attribute__((__packed__)) FAT
{
    uint16_t next;
};

struct __attribute__((__packed__)) rootdirectory
{

    char fileName[FS_FILENAME_LEN]; //(16 bytes) Filename
    uint32_t sizeOfFile;            //(4 bytes) Size of the files (in bytes)
    uint16_t firstIndex;            //(2 bytes) Index of the first data block
    uint8_t padding[10];            //(10 bytes) Unused/Padding
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
        // disk_error("No FS mounted");
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
        validFileName = false;
    }
    /*Error: filename too long*/
    if (lengthOfFilename > FS_FILENAME_LEN)
    {
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
            count++;
        }
    }
    if (count == 0)
    {
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
        fdValid = false;
    }
    return fdValid;
}

/*Finds the next empty FAT block*/
int emptyFATIndex(void)
{
    int nextEmptyFAT = 0;
    for (int i = 1; i < superBlock->numDataBlocks; i++)
    {
        if (fatArray[i].next == 0)
        {
            nextEmptyFAT = i;
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

/*Finds the file's location*/
int findFileLocation(int fd)
{
    int fileLocation = 0;
    for (int i = 0; i < FS_FILE_MAX_COUNT; i++)
    {
        if (strncmp(fdArray[fd].fileName, rootDirectory[i].fileName, FS_FILENAME_LEN) == 0)
        {
            fileLocation = i;
            break;
        }
    }
    return fileLocation;
}

/*MAIN FUNCTIONS */

int fs_mount(const char *diskname)
{
    // open disk
    // if virtual file disk cannot be openned return -1;
    if (block_disk_open(diskname) == -1)
    {
        return -1;
    }
    disk_open = true;

    // declaring a super block
    superBlock = malloc(BLOCK_SIZE);
    if (superBlock == NULL)
    {
        return -1;
    }

    if (block_read(0, (void *)superBlock) == -1)
    {
        return -1;
    }

    // checking signature
    if (strncmp(superBlock->signature, "ECS150FS", 8) != 0)
    {
        return -1;
    }

    if (block_disk_count() != superBlock->numBlockVirtualDisk)
    {
        return -1;
    }

    // fatArray initialization
    fatArray = malloc(superBlock->numBlocksFAT * BLOCK_SIZE);
    for (int i = 0; i < superBlock->numBlocksFAT; i++)
    {
        if (block_read(i + 1, (void *)fatArray + (i * BLOCK_SIZE)) == -1) // ask TA about
        {
            return -1;
        }
    }

    // rootDirectory initialization
    rootDirectory = malloc(sizeof(struct rootdirectory) * FS_FILE_MAX_COUNT);
    if (block_read(superBlock->numBlocksFAT + 1, (void *)rootDirectory) == -1) // ask TA about
    {
        return -1;
    }

    fdArray = malloc(sizeof(struct fdTable) * FD_MAX);

    return 0;
}

int fs_umount(void)
{
    /*Error: If no disk is opened, return -1 */
    if (disk_open == false)
    {
        return -1;
    }
    /*Error: No FS is currently mounted */
    if (!superBlock)
    {
        return -1;
    }

    // Writing metadata to the disk
    if (block_write(0, (void *)superBlock) == -1)
    {
        return -1;
    }

    // Writing file data to disk
    for (int i = 0; i < superBlock->numBlocksFAT; i++)
    {
        if (block_write(i + 1, (void *)fatArray + (i * BLOCK_SIZE)) == -1)
        {
            return -1;
        }
    }

    if (block_write(superBlock->numBlocksFAT + 1, (void *)rootDirectory) == -1)
    {
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
        if (rootDirectory[i].fileName[0] == '\0')
        {
            freeRootDirectoryCount++;
        }
    }
    if (freeRootDirectoryCount == 0)
    {
        return -1;
    }

    /*Go through the root directory and find the entry where everything is NULL*/
    for (int i = 0; i < FS_FILE_MAX_COUNT; i++)
    {

        // find empty spot in root directory
        if (rootDirectory[i].fileName[0] == '\0')
        {
            strcpy(rootDirectory[i].fileName, filename);
            rootDirectory[i].sizeOfFile = 0;
            rootDirectory[i].firstIndex = FAT_EOC;

            return 0;
        }
    }
    return -1;
}

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
        return -1;
    }

    /*all the data blocks containing the file’s contents must be freed in the FAT.*/
    for (int i = 0; i < FS_FILE_MAX_COUNT; i++)
    {
        if ((strlen(rootDirectory[i].fileName) > 0) && (strcmp(rootDirectory[i].fileName, filename) == 0))
        {
            rootDirectory[i].fileName[0] = '\0';
            if (rootDirectory[i].sizeOfFile > 0)
            {
                int fatIndex = rootDirectory[i].firstIndex;
                int nextFat;
                while (fatIndex != FAT_EOC)
                {
                    nextFat = fatArray[fatIndex].next;
                    fatArray[fatIndex].next = 0;
                    fatIndex = nextFat;

                } // end while
                rootDirectory[i].sizeOfFile = 0;
                // nextFat = 0;
            } // end if
        }     // end if
    }         // end for
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

        if (fdArray[i].empty == 0)
        {
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
    //fdArray[fd].fd = -1;
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

    int fdFileSize = -1;
    for (int i = 0; i < FS_FILE_MAX_COUNT; i++)
    {
        if (!strcmp(fdArray[fd].fileName, rootDirectory[i].fileName))
        {
            fdFileSize = rootDirectory[i].sizeOfFile;
            break;
        }
    }
    if (fdFileSize == -1)
    {
        return -1;
    }
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

    fdArray[fd].file_offset = offset;

    return 0;
}

int fs_write(int fd, void *buf, size_t count)
{
    // /*Error: No FS currently mounted, file descriptor is invalid,
    // 	or @buf is NULL*/
    if (checkIfFileOpen(superBlock) == 0 || checkFileDescriptorValid(fd) == 0 || buf == NULL)
    {
        return -1;
    }

    // /*Find the location of the file in the root directory to access its attributes */
    int fileLocation = findFileLocation(fd);

    /*Buffer to temporarily hold data that is going to be written to the file*/
    char bounceBuf[BLOCK_SIZE];
    int bounceBufOffSet = fdArray[fd].file_offset % BLOCK_SIZE;
    int currBlockNum = fdArray[fd].file_offset / BLOCK_SIZE;

    /*C. curFatBlockIndex*/
    int currentFATBlockIndex = findCurFatBlockIndex(fileLocation, currBlockNum);

    /*Buffer from where we are getting the data to write to the file*/
    char *writeBuf = (char *)buf;

    /*Find size of the file and the file's offset with @fd*/
    int fileSize = rootDirectory[fileLocation].sizeOfFile;
    int fileOffset = fdArray[fd].file_offset;
    int freeSpaceOnBlock = BLOCK_SIZE - fileSize;

    int totalBytesWritten = 0;
    int numOfBlocksToWrite = (count / BLOCK_SIZE) + 1; // new data blocks to allocate
    int bytesToWrite = 0;
    int bytesLeftToWrite = count;
    int nextEmptyFatIndex = emptyFATIndex();
    int spaceOnDisk = totalEmptyFATBlocks(); // total empty FAT blocks
    int firstFileOffSet = fileOffset;

    /*Step to take when there is no more space left on the file, so need to extend it*/
    if (currentFATBlockIndex == FAT_EOC)
    {
        /*set the current FAT block index to the next empty FAT index*/
        currentFATBlockIndex = nextEmptyFatIndex;
        rootDirectory[fileLocation].firstIndex = nextEmptyFatIndex;
        if (count == 0)
        {
            rootDirectory[fileLocation].firstIndex = FAT_EOC;
        }
    }

    /*Makes sure that the number of blocks to write is in boundary of space on the disk*/
    if (numOfBlocksToWrite > spaceOnDisk)
    {
        numOfBlocksToWrite = spaceOnDisk;
    }

    /*Write to file block by block*/
    for (int i = 0; i < numOfBlocksToWrite; i++)
    {
        /*file's offset is not aligned to the beginning of the block*/
        if ((fileOffset > 0) && (bytesLeftToWrite == count))
        {
            bytesToWrite = freeSpaceOnBlock;
            block_read(currentFATBlockIndex, bounceBuf);
            memcpy(bounceBuf + bounceBufOffSet, writeBuf, bytesToWrite);
            block_write(currentFATBlockIndex + superBlock->dataBlockStartIndex, bounceBuf);

            bytesLeftToWrite -= bytesToWrite;
            totalBytesWritten += bytesToWrite;
            fileOffset += bytesToWrite;

            /*Set the current FAT block's next pointer to the next empty FAT block, if there is more bytes left to write*/
            if (bytesLeftToWrite != 0)
            {
                nextEmptyFatIndex = emptyFATIndex();
                fatArray[currentFATBlockIndex].next = nextEmptyFatIndex;
                currentFATBlockIndex = fatArray[currentFATBlockIndex].next;
            }
            else /*Sets current FAT block's next pointer to EOC*/
            {
                nextEmptyFatIndex = FAT_EOC;
                fatArray[currentFATBlockIndex].next = nextEmptyFatIndex;
            }
            break;
        }

        /*In case of the need to write partially to the block, and not in entirety*/
        if (bytesLeftToWrite < BLOCK_SIZE)
        {
            memcpy(bounceBuf, writeBuf, bytesLeftToWrite);
            block_write(currentFATBlockIndex + superBlock->dataBlockStartIndex, bounceBuf);

            totalBytesWritten += bytesLeftToWrite;
            fileOffset += bytesLeftToWrite;
            bytesLeftToWrite -= bytesLeftToWrite;

            nextEmptyFatIndex = FAT_EOC;
            fatArray[currentFATBlockIndex].next = nextEmptyFatIndex;
            break;
        }

        /*files's offset is aligned perfectly to the beginning of the block
         AND bytesLeftToWrite is greater than or equal to BLOCK_SIZE*/
        memcpy(bounceBuf, writeBuf, BLOCK_SIZE);
        block_write(currentFATBlockIndex + superBlock->dataBlockStartIndex, bounceBuf);

        bytesLeftToWrite -= BLOCK_SIZE;
        totalBytesWritten += BLOCK_SIZE;
        fileOffset += BLOCK_SIZE;

        /*Set the current FAT block's next pointer to the next empty FAT block, if there is more bytes left to write*/
        if (bytesLeftToWrite != 0)
        {
            fatArray[currentFATBlockIndex].next = 1;
            nextEmptyFatIndex = emptyFATIndex();
            fatArray[currentFATBlockIndex].next = nextEmptyFatIndex;
            currentFATBlockIndex = fatArray[currentFATBlockIndex].next;
        }
        else /*Sets current FAT block's next pointer to EOC*/
        {
            nextEmptyFatIndex = FAT_EOC;
            fatArray[currentFATBlockIndex].next = nextEmptyFatIndex;
        }
    }
    currentFATBlockIndex = fatArray[currentFATBlockIndex].next;
    rootDirectory[fileLocation].sizeOfFile = firstFileOffSet + totalBytesWritten;

    return totalBytesWritten;
}

int fs_read(int fd, void *buf, size_t count)
{
    /*Error: No FS currently mounted, file descriptor is invalid,
        or @buf is NULL*/
    if (checkIfFileOpen(superBlock) == 0 || checkFileDescriptorValid(fd) == 0 || buf == NULL)
    {
        return -1;
    }

    /*Find the location of the file in the root directory to access its attributes */
    int fileLocation = findFileLocation(fd);

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

    /*Create bounce buffer*/
    char bounceBuf[BLOCK_SIZE];
    char *readBuf = (char *)buf;
    int bounceBufOffSet = fdArray[fd].file_offset % BLOCK_SIZE;
    int currBlockNum = fdArray[fd].file_offset / BLOCK_SIZE;

    /*Find current FAT block index*/
    int currentFATBlockIndex = findCurFatBlockIndex(fileLocation, currBlockNum);

    /*Variable to store the number of bytes actually read*/
    int numBytesRead = 0;
    int numOfBlocksToRead = (countOfBytesToRead / BLOCK_SIZE) + 1;

    if (countOfBytesToRead <= BLOCK_SIZE) // reading from a single block
    {
        block_read(currentFATBlockIndex + superBlock->dataBlockStartIndex, bounceBuf);
        memcpy(readBuf, bounceBuf + bounceBufOffSet, countOfBytesToRead);
        numBytesRead += countOfBytesToRead;
        fdArray[fd].file_offset += countOfBytesToRead;
    }
    else if (countOfBytesToRead > BLOCK_SIZE) // reading from multiple blocks
    {
        int firstLoopEntered = 0;

        /*All blocks are read in entirety except maybe for the last one*/
        if (fdArray[fd].file_offset == 0)
        {
            firstLoopEntered = 1;
            for (int i = 0; i < numOfBlocksToRead; i++)
            {
                /*Reads from the last block*/
                if (countOfBytesToRead < BLOCK_SIZE)
                {
                    // printf("Read CP10\n");
                    char *endBuff[countOfBytesToRead - 1];
                    block_read(currentFATBlockIndex + superBlock->dataBlockStartIndex, endBuff);
                    memcpy(readBuf, endBuff, countOfBytesToRead);
                    numBytesRead += countOfBytesToRead;
                    fdArray[fd].file_offset += countOfBytesToRead;
                    break;
                }

                /*Reads from all blocks except the last*/
                block_read(currentFATBlockIndex + superBlock->dataBlockStartIndex, readBuf);
                numBytesRead += BLOCK_SIZE;
                fdArray[fd].file_offset += BLOCK_SIZE;
                countOfBytesToRead = abs(countOfBytesToRead - BLOCK_SIZE);  // 9000-4096 = 904
                currentFATBlockIndex = fatArray[currentFATBlockIndex].next; // block 2
            }
        }

        /*First block (and maybe the last block) is read partially */
        if (fdArray[fd].file_offset > 0 && firstLoopEntered == 0)
        {
            /*First block read partially*/
            block_read(currentFATBlockIndex + superBlock->dataBlockStartIndex, bounceBuf);
            numBytesRead = BLOCK_SIZE - bounceBufOffSet;
            memcpy(readBuf, bounceBuf + bounceBufOffSet, numBytesRead);
            currentFATBlockIndex = fatArray[currentFATBlockIndex].next;
            fdArray[fd].file_offset += numBytesRead;
            countOfBytesToRead = countOfBytesToRead - numBytesRead;

            /*Rest of the blocks being ready in entirety except maybe for the last one*/
            for (int i = 0; i < numOfBlocksToRead; i++)
            {
                block_read(currentFATBlockIndex + superBlock->dataBlockStartIndex, readBuf);
                numBytesRead += BLOCK_SIZE;
                currentFATBlockIndex = fatArray[currentFATBlockIndex].next;
                fdArray[fd].file_offset += BLOCK_SIZE;
                countOfBytesToRead = countOfBytesToRead - numBytesRead;

                /*If the last block is not read in entirety*/
                if (countOfBytesToRead < BLOCK_SIZE)
                {
                    block_read(currentFATBlockIndex + superBlock->dataBlockStartIndex, bounceBuf);
                    memcpy(readBuf, bounceBuf + bounceBufOffSet, countOfBytesToRead);
                    numBytesRead += countOfBytesToRead;
                    fdArray[fd].file_offset += countOfBytesToRead;
                } // end if

            } // end for
        }
    }
    return numBytesRead;
}
