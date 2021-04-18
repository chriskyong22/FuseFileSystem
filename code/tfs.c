/*
 *  Copyright (C) 2021 CS416 Rutgers CS
 *	Tiny File System
 *	File:	tfs.c
 *
 */

#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/time.h>
#include <libgen.h>
#include <limits.h>

#include "block.h"
#include "tfs.h"

unsigned long customCeil(double num);
#define SUPERBLOCK_BLOCK 0
#define INODE_BITMAP_BLOCK 1
#define DATA_BITMAP_BLOCK 2
#define INODE_REGION_BLOCK 3
#define FILE_TYPE 0
#define DIRECTORY_TYPE 1
#define HARD_LINK_TYPE 2
#define SYMBIOTIC_LINK_TYPE 3

char diskfile_path[PATH_MAX];
struct superblock superBlock;
static const struct dirent emptyDirentStruct;

// Declare your in-memory data structures here



/* 
 * Get available inode number from bitmap
 */
int get_avail_ino() {

	// Step 1: Read inode bitmap from disk
	
	// Step 2: Traverse inode bitmap to find an available slot

	// Step 3: Update inode bitmap and write to disk 
	const int CHAR_IN_BITS = sizeof(char) * 8;
	const int BYTE_MASK = (1 << (CHAR_IN_BITS)) - 1; 
	unsigned int bitmapBlock = superBlock.i_bitmap_blk;
	char* bitMap = malloc(sizeof(char) * BLOCK_SIZE);
	bio_read(bitmapBlock, bitMap);
	unsigned int maxByte = MAX_INUM / 8.0;
	for(unsigned int byteIndex = 0; byteIndex < maxByte; byteIndex++) {
		char* byteLocation = (bitMap + byteIndex);
		// For each char, mask it to see if there is a free inode within the char
		// if there is a free inode within a char, the char will not equal 255. 
		if (((*byteLocation) & BYTE_MASK) != BYTE_MASK) {
			for(int bitIndex = 0; bitIndex < CHAR_IN_BITS; bitIndex++) {
			/*
				bitMask values ~ 0b1 = 1, 0b10 = 2, 0b100 = 4, 0b1000 = 8
				0b10000 = 16, 0b100000 = 32, 0b1000000 = 64, 0b10000000 = 128
			*/
				int bitMask = 1 << bitIndex;
				if(((*byteLocation) & bitMask) == 0) {
					// The iNode Number is (byteIndex * 8) + bitIndex.
					// Since each byte hold 8 inodes, then bitIndex
					// indicates a inode within a char.
					(*byteLocation) |= bitMask;
					bio_write(bitmapBlock, bitMap);
					free(bitMap);
					return (byteIndex * 8) + bitIndex;
				}
			}
		}
	}
	
	return 0;
}

/* 
 * Get available data block number from bitmap
 */
int get_avail_blkno() {

	// Step 1: Read data block bitmap from disk
	
	// Step 2: Traverse data block bitmap to find an available slot

	// Step 3: Update data block bitmap and write to disk 
	const int CHAR_IN_BITS = sizeof(char) * 8;
	const int BYTE_MASK = (1 << (CHAR_IN_BITS)) - 1; 
	unsigned int bitmapBlock = superBlock.d_bitmap_blk;
	char* bitMap = malloc(sizeof(char) * BLOCK_SIZE);
	bio_read(bitmapBlock, bitMap);
	unsigned int maxByte = MAX_DNUM / 8.0;
	for(unsigned long byteIndex = 0; byteIndex < maxByte; byteIndex++) {
		char* byteLocation = (bitMap + byteIndex);
		// For each char, mask it to see if there is a free datablock within the char
		// if there is a free datablock within a char, the char will not equal 255. 
		if (((*byteLocation) & BYTE_MASK) != BYTE_MASK) {
			for(int bitIndex = 0; bitIndex < CHAR_IN_BITS; bitIndex++) {
			/*
				bitMask values ~ 0b1 = 1, 0b10 = 2, 0b100 = 4, 0b1000 = 8
				0b10000 = 16, 0b100000 = 32, 0b1000000 = 64, 0b10000000 = 128
			*/
				int bitMask = 1 << bitIndex;
				if(((*byteLocation) & bitMask) == 0) {
					// The data Number is (byteIndex * 8) + bitIndex.
					// Since each byte hold 8 inodes, then bitIndex
					// indicates a datablock within a char.
					(*byteLocation) |= bitMask;
					bio_write(bitmapBlock, bitMap);
					free(bitMap);
					return (byteIndex * 8) + bitIndex;
				}
			}
		}
	}
	
	return 0;
}

/* 
 * inode operations
 */
int readi(uint16_t ino, struct inode *inode) {

  // Step 1: Get the inode's on-disk block number

  // Step 2: Get offset of the inode in the inode on-disk block

  // Step 3: Read the block from disk and then copy into inode structure
	
	unsigned int numberOfINodesPerBlock = BLOCK_SIZE / sizeof(struct inode);
	unsigned int blockNumber = ino / numberOfINodesPerBlock;
	int iNode_blockNumber = superBlock.i_start_blk + blockNumber;
	char* buffer = malloc(sizeof(BLOCK_SIZE));
	bio_read(iNode_blockNumber, buffer); 
	memcpy(inode, buffer + (sizeof(struct inode) * (ino % numberOfINodesPerBlock)),sizeof(struct inode));
	free(buffer);
	return 0;
}

int writei(uint16_t ino, struct inode *inode) {

	// Step 1: Get the block number where this inode resides on disk
	
	// Step 2: Get the offset in the block where this inode resides on disk

	// Step 3: Write inode to disk 
	unsigned int numberOfINodesPerBlock = BLOCK_SIZE / sizeof(struct inode);
	unsigned int blockNumber = ino / numberOfINodesPerBlock;
	int iNode_blockNumber = superBlock.i_start_blk + blockNumber;
	char* buffer = malloc(sizeof(BLOCK_SIZE));
	bio_read(iNode_blockNumber, buffer);
	memcpy(buffer + (sizeof(struct inode) * (ino % numberOfINodesPerBlock)), inode, sizeof(struct inode));
	bio_write(iNode_blockNumber, buffer); 
	free(buffer);
	
	return 0;
}

int readDirectoryBlock (char* datablock, struct dirent *dirEntry, const char *fname, size_t name_len) {
	uint32_t maxDirentIndex = BLOCK_SIZE / (sizeof(struct dirent));
	for(int directIndex = 0; directIndex < maxDirentIndex; directIndex++) {
		(*dirEntry) = emptyDirentStruct;
		memcpy(dirEntry, datablock + (directIndex * (sizeof(struct dirent))), sizeof(struct dirent));
		if (strcmp(dirEntry->name, fname) == 0) {
			return 1;
		}
	}
	return -1;
}

/* 
 * directory operations
 */
int dir_find(uint16_t ino, const char *fname, size_t name_len, struct dirent *dirent) {

  // Step 1: Call readi() to get the inode using ino (inode number of current directory)

  // Step 2: Get data block of current directory from inode

  // Step 3: Read directory's data block and check each directory entry.
  //If the name matches, then copy directory entry to dirent structure
	struct inode dirINode;
	readi(ino, &dirINode);

	if (dirINode.type != DIRECTORY_TYPE) {
		printf("[E]: Passed in I-Number was not type directory but type %d!\n", dirINode.type); 
	}
	
	uint32_t size = dirINode.size;
	uint32_t allocatedBlocks = size / BLOCK_SIZE;
	uint32_t maxDirectSize = 16 * BLOCK_SIZE;
	uint32_t iteration = size < maxDirectSize ? customCeil(size / BLOCK_SIZE) : 16;
	char* buffer = calloc(1, BLOCK_SIZE);
	for(int directPointerIndex = 0; directPointerIndex < iteration; directPointerIndex++) {
		// Currently assuming the direct ptrs are block locations and not memory addressses 
		if (dirINode.direct_ptr[directPointerIndex] != 0) {
			bio_read(dirINode.direct_ptr[directPointerIndex], buffer);
			readDirectoryBlock(buffer, dirent, fname, name_len);
			// READ IN BLOCK
			// Traverse by sizeof(dirent)
			// For each iteration, check if the dirent structure has fname and if so, copy to dirent
		}
	}
	size -= maxDirectSize;
	uint32_t maxIndirectSize = 8 * BLOCK_SIZE * BLOCK_SIZE;
	iteration = size < maxIndirectSize ? customCeil(size / (BLOCK_SIZE * BLOCK_SIZE)) : 8;
	
	for(int indirectPointerIndex = 0; indirectPointerIndex < iteration; indirectPointerIndex++) {
		// READ IN INDIRECT BLOCK (CONTAINS BLOCK_SIZE/ sizeof(uint32_t) pointers to data blocks that contain DATA)
		// FOR EACH VALID DATA BLOCK -> Traverse by sizeof(dirent)
		// For each iteration, check if the dirent structure has fname and if so, copy to dirent
	}
	
	// If reached this point, could not find the directory entry given the ino
	(*dirent) = emptyDirentStruct;
	return -1;
}

int dir_add(struct inode dir_inode, uint16_t f_ino, const char *fname, size_t name_len) {

	// Step 1: Read dir_inode's data block and check each directory entry of dir_inode
	
	// Step 2: Check if fname (directory name) is already used in other entries

	// Step 3: Add directory entry in dir_inode's data block and write to disk

	// Allocate a new data block for this directory if it does not exist

	// Update directory inode

	// Write directory entry
	
	
	return 0;
}

int dir_remove(struct inode dir_inode, const char *fname, size_t name_len) {

	// Step 1: Read dir_inode's data block and checks each directory entry of dir_inode
	
	// Step 2: Check if fname exist

	// Step 3: If exist, then remove it from dir_inode's data block and write to disk
	uint32_t size = dir_inode.size;
	uint32_t allocatedBlocks = size / BLOCK_SIZE;
	uint32_t maxDirectSize = 16 * BLOCK_SIZE;
	uint32_t iteration = size < maxDirectSize ? customCeil(size / BLOCK_SIZE) : 16;
	for(int directPointerIndex = 0; directPointerIndex < iteration; directPointerIndex++) {
		if (dir_inode.direct_ptr[directPointerIndex] != 0) {
			// READ IN BLOCK
			// Traverse by sizeof(dirent)
			// For each iteration, check if the dirent structure has fname and if so, memcopy to null it out and then write to block
		}
	}
	size -= maxDirectSize;
	uint32_t maxIndirectSize = 8 * BLOCK_SIZE * BLOCK_SIZE;
	iteration = size < maxIndirectSize ? customCeil(size / (BLOCK_SIZE * BLOCK_SIZE)) : 8;
	
	for(int indirectPointerIndex = 0; indirectPointerIndex < iteration; indirectPointerIndex++) {
		// READ IN INDIRECT BLOCK (CONTAINS BLOCK_SIZE/ sizeof(uint32_t) pointers to data blocks that contain DATA)
		// FOR EACH VALID DATA BLOCK -> Traverse by sizeof(dirent)
		// For each iteration, check if the dirent structure has fname and if so, memcopy to null it out and then write to block
	}
	return -1;
}

/* 
 * namei operation
 */
int get_node_by_path(const char *path, uint16_t ino, struct inode *inode) {
	
	// Step 1: Resolve the path name, walk through path, and finally, find its inode.
	// Note: You could either implement it in a iterative way or recursive way
	
	return 0;
}

/* 
 * Make file system
 */
int tfs_mkfs() {

	// Call dev_init() to initialize (Create) Diskfile
	
	// write superblock information

	// initialize inode bitmap
		
	// initialize data block bitmap

	// update bitmap information for root directory

	// update inode for root directory
	
	dev_init(diskfile_path);
	
	superBlock.magic_num = MAGIC_NUM;
	superBlock.max_inum = MAX_INUM;
	superBlock.max_dnum = MAX_DNUM;
	superBlock.i_bitmap_blk = INODE_BITMAP_BLOCK;
	superBlock.d_bitmap_blk = DATA_BITMAP_BLOCK;
	superBlock.i_start_blk = INODE_REGION_BLOCK;
	// INode Regions starts blockIndex 3 and spans across MAX_INUM / (BLOCK_SIZE/ INODE SIZE) therefore
	// + 1 to get next unused block or where datablock region starts 
	superBlock.d_start_blk = 1 + INODE_REGION_BLOCK + customCeil((MAX_INUM * 1.0) / (BLOCK_SIZE / sizeof(struct inode)));
	
	char* superblockBuffer = calloc(1, BLOCK_SIZE);
	memcpy(superblockBuffer, &superBlock, sizeof(struct superblock));
	bio_write(SUPERBLOCK_BLOCK, superblockBuffer);
	free(superblockBuffer);
	char* bitMap = calloc(1, BLOCK_SIZE);
	bio_write(INODE_BITMAP_BLOCK, bitMap);
	bio_write(DATA_BITMAP_BLOCK, bitMap);
	//TO DO UPDATE BITMAP INFO for ROOT and INODE for ROOT 
	free(bitMap);
	return 0;
}


/* 
 * FUSE file operations
 */
static void *tfs_init(struct fuse_conn_info *conn) {

	// Step 1a: If disk file is not found, call mkfs

  // Step 1b: If disk file is found, just initialize in-memory data structures
  // and read superblock from disk
	if (dev_open(diskfile_path) == -1) {
		tfs_mkfs();
	} else {
		char* superblockBuffer = calloc(1, BLOCK_SIZE);
		bio_read(SUPERBLOCK_BLOCK, superblockBuffer);
		memcpy(&superBlock, superblockBuffer, sizeof(struct superblock));
		free(superblockBuffer);
	}
	return NULL;
}

static void tfs_destroy(void *userdata) {

	// Step 1: De-allocate in-memory data structures

	// Step 2: Close diskfile
	dev_close();
}

static int tfs_getattr(const char *path, struct stat *stbuf) {

	// Step 1: call get_node_by_path() to get inode from path

	// Step 2: fill attribute of file into stbuf from inode

	stbuf->st_mode   = S_IFDIR | 0755;
	stbuf->st_nlink  = 2;
	time(&stbuf->st_mtime);

	return 0;
}

static int tfs_opendir(const char *path, struct fuse_file_info *fi) {

	// Step 1: Call get_node_by_path() to get inode from path

	// Step 2: If not find, return -1

    return 0;
}

static int tfs_readdir(const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {

	// Step 1: Call get_node_by_path() to get inode from path

	// Step 2: Read directory entries from its data blocks, and copy them to filler

	return 0;
}


static int tfs_mkdir(const char *path, mode_t mode) {

	// Step 1: Use dirname() and basename() to separate parent directory path and target directory name

	// Step 2: Call get_node_by_path() to get inode of parent directory

	// Step 3: Call get_avail_ino() to get an available inode number

	// Step 4: Call dir_add() to add directory entry of target directory to parent directory

	// Step 5: Update inode for target directory

	// Step 6: Call writei() to write inode to disk
	

	return 0;
}

static int tfs_rmdir(const char *path) {

	// Step 1: Use dirname() and basename() to separate parent directory path and target directory name

	// Step 2: Call get_node_by_path() to get inode of target directory

	// Step 3: Clear data block bitmap of target directory

	// Step 4: Clear inode bitmap and its data block

	// Step 5: Call get_node_by_path() to get inode of parent directory

	// Step 6: Call dir_remove() to remove directory entry of target directory in its parent directory

	return 0;
}

static int tfs_releasedir(const char *path, struct fuse_file_info *fi) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
    return 0;
}

static int tfs_create(const char *path, mode_t mode, struct fuse_file_info *fi) {

	// Step 1: Use dirname() and basename() to separate parent directory path and target file name

	// Step 2: Call get_node_by_path() to get inode of parent directory

	// Step 3: Call get_avail_ino() to get an available inode number

	// Step 4: Call dir_add() to add directory entry of target file to parent directory

	// Step 5: Update inode for target file

	// Step 6: Call writei() to write inode to disk

	return 0;
}

static int tfs_open(const char *path, struct fuse_file_info *fi) {

	// Step 1: Call get_node_by_path() to get inode from path

	// Step 2: If not find, return -1

	return 0;
}

static int tfs_read(const char *path, char *buffer, size_t size, off_t offset, struct fuse_file_info *fi) {

	// Step 1: You could call get_node_by_path() to get inode from path

	// Step 2: Based on size and offset, read its data blocks from disk

	// Step 3: copy the correct amount of data from offset to buffer

	// Note: this function should return the amount of bytes you copied to buffer
	return 0;
}

static int tfs_write(const char *path, const char *buffer, size_t size, off_t offset, struct fuse_file_info *fi) {
	// Step 1: You could call get_node_by_path() to get inode from path

	// Step 2: Based on size and offset, read its data blocks from disk

	// Step 3: Write the correct amount of data from offset to disk

	// Step 4: Update the inode info and write it to disk

	// Note: this function should return the amount of bytes you write to disk
	return size;
}

static int tfs_unlink(const char *path) {

	// Step 1: Use dirname() and basename() to separate parent directory path and target file name

	// Step 2: Call get_node_by_path() to get inode of target file

	// Step 3: Clear data block bitmap of target file

	// Step 4: Clear inode bitmap and its data block

	// Step 5: Call get_node_by_path() to get inode of parent directory

	// Step 6: Call dir_remove() to remove directory entry of target file in its parent directory

	return 0;
}

static int tfs_truncate(const char *path, off_t size) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
    return 0;
}

static int tfs_release(const char *path, struct fuse_file_info *fi) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
	return 0;
}

static int tfs_flush(const char * path, struct fuse_file_info * fi) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
    return 0;
}

static int tfs_utimens(const char *path, const struct timespec tv[2]) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
    return 0;
}


static struct fuse_operations tfs_ope = {
	.init		= tfs_init,
	.destroy	= tfs_destroy,

	.getattr	= tfs_getattr,
	.readdir	= tfs_readdir,
	.opendir	= tfs_opendir,
	.releasedir	= tfs_releasedir,
	.mkdir		= tfs_mkdir,
	.rmdir		= tfs_rmdir,

	.create		= tfs_create,
	.open		= tfs_open,
	.read 		= tfs_read,
	.write		= tfs_write,
	.unlink		= tfs_unlink,

	.truncate   = tfs_truncate,
	.flush      = tfs_flush,
	.utimens    = tfs_utimens,
	.release	= tfs_release
};

unsigned long customCeil(double num) {
	unsigned long floor = (unsigned long) num;
	return (num == floor) ? floor : floor + 1;
}

int main(int argc, char *argv[]) {
	int fuse_stat;

	getcwd(diskfile_path, PATH_MAX);
	strcat(diskfile_path, "/DISKFILE");

	fuse_stat = fuse_main(argc, argv, &tfs_ope, NULL);

	return fuse_stat;
}

