#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "disk.h"
#include "fs.h"

/* TODO: Phase 1 */

struct __attribute__((__packed__)) superblock
{
	/*
		(8 characters) signature
		(2 bytes) total amount of blocks of virtual disk
		(2 bytes) root directory block index
		(2 bytes) data block start index
		(2 bytes) amount of data blcoks
		(1 bytes) number of blocks for FAT
		(4079 characters?) unsused/padding
	*/
};

struct __attribute__((__packed__)) FAT
{
	/*
		16-bit unsigned words as many entries as data blocks in disk
		kinda confused on what goes inside here...
	*/
};

struct __attribute__((__packed__)) rootdirectory
{
	/*
		(16 bytes) Filename
		(4 bytes) Size of the files (in bytes)
		(2 bytes) Index of the first data block
		(10 bytes) Unused/Padding
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

