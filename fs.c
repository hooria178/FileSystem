#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdin.h>

#include "disk.h"
#include "fs.h"

/* TODO: Phase 1 */

struct __attribute__((__packed__)) superblock
{
	/*
		char signature[8] (8 characters) signature
		uint16_t numBlockVirtualDisk(2 bytes) total amount of blocks of virtual disk
		uint16_t rootBlockIndex(2 bytes) root directory block index
		uint16_t dataBlockStartIndex(2 bytes) data block start index
		uint16_t numDataBlocks(2 bytes) amount of data blcoks
		uint8_t numBlocksFAT(1 bytes) number of blocks for FAT
		uint32632_t padding[32632] (4079 bytes) unsused/padding  ASK THIS 
	*/
};
/*struct node {
	//uint16_t data //16-bit wide data
	//int index
	//struct node *next // pointer to next node
}
*/

struct __attribute__((__packed__)) FAT
{
	//node *front;
	//node *rear

	/*
		16-bit unsigned words as many entries as data blocks in disk
		kinda confused on what goes inside here...
		//16-bit entries - make nodes of linked list 
		
	*/
	/*struct *node = (struct node*)malloc(sizeof(struct node));
	node->data = 0xFFFF;
	node->next = NULL;
	node->index = 0
	FAT->front = node; //node with data 0xFFFF becomes the first node at index 0
	FAT->rear = NULL;
	
	*/
};

struct __attribute__((__packed__)) rootdirectory
{
	/*
		char fileName[16] (16 bytes) Filename
		unint32_t sizeOfFile(4 bytes) Size of the files (in bytes)
		unint16_t firstIndex (2 bytes) Index of the first data block
		unit80_t padding[80] (10 bytes) Unused/Padding
	*/
};

int fs_mount(const char *diskname)
{
	/* TODO: Phase 1 */
}

int fs_umount(void)
{
	/* TODO: Phase 1 */
}

int fs_info(void)
{
	/* TODO: Phase 1 */
}

int fs_create(const char *filename)
{
	/* TODO: Phase 2 */
}

int fs_delete(const char *filename)
{
	/* TODO: Phase 2 */
}

int fs_ls(void)
{
	/* TODO: Phase 2 */
}

int fs_open(const char *filename)
{
	/* TODO: Phase 3 */
}

int fs_close(int fd)
{
	/* TODO: Phase 3 */
}

int fs_stat(int fd)
{
	/* TODO: Phase 3 */
}

int fs_lseek(int fd, size_t offset)
{
	/* TODO: Phase 3 */
}

int fs_write(int fd, void *buf, size_t count)
{
	/* TODO: Phase 4 */
}

int fs_read(int fd, void *buf, size_t count)
{
	/* TODO: Phase 4 */
}