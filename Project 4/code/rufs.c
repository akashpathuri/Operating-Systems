/*
 *  Copyright (C) 2022 CS416/518 Rutgers CS
 *	RU File System
 *	File:	rufs.c
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
//#include <math.h>

#include "block.h"
#include "rufs.h"

char diskfile_path[PATH_MAX];

// Declare your in-memory data structures here
const int disk_block = 0;

struct superblock* super_block;
int inodes_per_block;
int dirent_per_block;

/* 
 * Get available inode number from bitmap
 */
int get_avail_ino() {

	// Step 1: Read inode bitmap from disk
	bitmap_t inode_bitmap = malloc(BLOCK_SIZE);
	bio_read(super_block->i_bitmap_blk, inode_bitmap);

	// Step 2: Traverse inode bitmap to find an available slot
	for(int i=0; i<MAX_INUM; i++){
		// Step 3: Update inode bitmap and write to disk 	
		if(!get_bitmap(inode_bitmap, i)){
			set_bitmap(inode_bitmap, i);
			bio_write(super_block->i_bitmap_blk, inode_bitmap);
			free(inode_bitmap);
			return i;
		}
	}
	
	//Not Found
	return -1;
}

/* 
 * Get available data block number from bitmap
 */
int get_avail_blkno() {

	// Step 1: Read data block bitmap from disk
	bitmap_t data_block_bitmap = malloc(BLOCK_SIZE);
	bio_read(super_block->d_bitmap_blk, data_block_bitmap);

	// Step 2: Traverse data block bitmap to find an available slot
	for(int i=0; i<MAX_DNUM; i++){
		// Step 3: Update data block bitmap and write to disk 	
		if(!get_bitmap(data_block_bitmap, i)){
			set_bitmap(data_block_bitmap, i);
			bio_write(super_block->d_bitmap_blk, data_block_bitmap);
			free(data_block_bitmap);
			return i;
		}
	}
	
	//Not Found
	return -1;
}

/* 
 * inode operations
 */
int readi(uint16_t ino, struct inode *inode) {

  	// Step 1: Get the inode's on-disk block number
	int block_number = super_block->i_start_blk;
	block_number+= ino/inodes_per_block;

	void *block = malloc(BLOCK_SIZE);
	if(bio_read(block_number, block)<0)
		return 0;

	// Step 2: Get offset of the inode in the inode on-disk block
	int offset=(ino%inodes_per_block)*sizeof(struct inode);

	// Step 3: Read the block from disk and then copy into inode structure
	memcpy(inode, block+offset, sizeof(struct inode));
	free(block);
	return 1;
}

int writei(uint16_t ino, struct inode *inode) {

	// Step 1: Get the block number where this inode resides on disk
	int block_number = super_block->i_start_blk;
	block_number+= ino/inodes_per_block;

	void *block = malloc(BLOCK_SIZE);
	if(bio_read(block_number, block)<0)
		return 0;

	// Step 2: Get the offset in the block where this inode resides on disk
	int offset=(ino%inodes_per_block)*sizeof(struct inode);

	// Step 3: Write inode to disk 
	memcpy(block+offset, inode, sizeof(struct inode));
	return bio_write(block_number, block);
	// if(bio_write(block_number, block)<0)
	// 	return 0;

	// free(block);
	// return 1;
}


/* 
 * directory operations
 */
int dir_find(uint16_t ino, const char *fname, size_t name_len, struct dirent *dirent) {

  // Step 1: Call readi() to get the inode using ino (inode number of current directory)
	struct inode *directory_inode = malloc(sizeof(struct inode));
	if(readi(ino, directory_inode)==-1){
		free(directory_inode);
		return -1;
	}

  // Step 2: Get data block of current directory from inode
	int *directory_data = directory_inode->direct_ptr;
  // Step 3: Read directory's data block and check each directory entry.
  //If the name matches, then copy directory entry to dirent structure
  	int* data_buffer = malloc(BLOCK_SIZE);
	for(int x = 0; x<16; x++){
		int d_ptr = directory_data[x];
		if(d_ptr >= 0){
			int block_number = super_block->d_start_blk;
			block_number += d_ptr;
			if(bio_read(block_number, data_buffer)<=0)
				continue;
			for(int y = 0; y<dirent_per_block; y++){
				struct dirent *directory_entry = (struct dirent *)data_buffer+y;
				if(directory_entry!=NULL && directory_entry->valid &&!strcmp(directory_entry->name, fname)){
					*dirent=*directory_entry;
					free(directory_inode);
					free(data_buffer);
					return d_ptr;
				}
			}
		}
	}
	free(directory_inode);
	free(data_buffer);
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

	return 0;
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
int rufs_mkfs() {

	// Call dev_init() to initialize (Create) Diskfile
	dev_init(diskfile_path);
	dev_open(diskfile_path);

	// write superblock information
	super_block = malloc(sizeof(struct superblock));
	super_block->magic_num = MAGIC_NUM;
	super_block->max_inum = MAX_INUM;
	super_block->max_dnum = MAX_DNUM;
	super_block->i_start_blk = 3;
	int inode_count = MAX_INUM/inodes_per_block;
	if(MAX_INUM%inodes_per_block)
		inode_count++;

	super_block->d_start_blk = (super_block->i_start_blk)+inode_count;

	// initialize inode bitmap
	bitmap_t inode_bitmap =(bitmap_t) malloc(MAX_INUM / 8);
	super_block->i_bitmap_blk = 1;
	
	// initialize data block bitmap
	bitmap_t data_block =(bitmap_t) malloc(MAX_DNUM / 8);;
	super_block->d_bitmap_blk = 2;

	// update bitmap information for root directory
	bio_write(super_block->i_bitmap_blk, inode_bitmap);
	bio_write(super_block->d_bitmap_blk, data_block);

	// update inode for root directory
	struct inode* root = calloc(1,sizeof(struct inode));
	//initialize_dir_inode(root);
	writei(disk_block,root);

	return 0;
}


/* 
 * FUSE file operations
 */
static void *rufs_init(struct fuse_conn_info *conn) {
	//initiliaze global variables
	inodes_per_block = BLOCK_SIZE/sizeof(struct inode);
	dirent_per_block = BLOCK_SIZE/sizeof(struct dirent);

  // Step 1a: If disk file is not found, call mkfs
	if(dev_open(diskfile_path)==-1){
		rufs_mkfs();
	}
  // Step 1b: If disk file is found, just initialize in-memory data structures
  // and read superblock from disk
	else{
		super_block = malloc(sizeof(struct superblock));
		bio_read(disk_block, super_block);
	}

	return NULL;
}

static void rufs_destroy(void *userdata) {

	// Step 1: De-allocate in-memory data structures
	free(super_block);

	// Step 2: Close diskfile
	dev_close();

}

static int rufs_getattr(const char *path, struct stat *stbuf) {

	// Step 1: call get_node_by_path() to get inode from path
	struct inode *inode = malloc(sizeof(struct inode));
	get_node_by_path(path, 0, inode);

	// Step 2: fill attribute of file into stbuf from inode

	stbuf->st_mode   = S_IFDIR | 0755;
	stbuf->st_nlink  = 2;
	stbuf->st_uid = getuid();
	stbuf->st_gid = getgid();
	stbuf->st_size = inode->size;
	time(&stbuf->st_mtime);

	return 0;
}

static int rufs_opendir(const char *path, struct fuse_file_info *fi) {

	// Step 1: Call get_node_by_path() to get inode from path
	struct inode *inode = malloc(sizeof(struct inode));
	if(get_node_by_path(path, 0, inode)==-1){
		// Step 2: If not find, return -1
		free(inode);
		return -1;
	}

	free(inode);
    return 0;
}

static int rufs_readdir(const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {

	// Step 1: Call get_node_by_path() to get inode from path
	struct inode *inode = malloc(sizeof(struct inode));
	if(get_node_by_path(path, 0, inode)==-1){
		free(inode);
		return -1;
	}
	filler(buffer, ".", NULL,offset); // Current Directory
	filler(buffer, "..", NULL,offset); // Parent Directory
	// Step 2: Read directory entries from its data blocks, and copy them to filler
	struct dirent* data_buffer=malloc(BLOCK_SIZE);
	for(int x = 0; x<16; x++){
		int d_ptr = inode->direct_ptr[x];
		if(d_ptr >= 0){
			int block_number = super_block->d_start_blk;
			block_number += d_ptr;
			if(bio_read(block_number, data_buffer)<=0)
				continue;
			
			for(int y = 0; y<dirent_per_block; y++){
				if(data_buffer[y].valid){
					if(filler(buffer, data_buffer[y].name,NULL,offset))
						break;
				}
			}

		}
	}
	free(inode);
	free(data_buffer);
	return 0;
}

static int rufs_mkdir(const char *path, mode_t mode) {

	// Step 1: Use dirname() and basename() to separate parent directory path and target directory name
	char *parent_directory = dirname((char*) path);
	char *target_directory = basename((char*) path);

	// Step 2: Call get_node_by_path() to get inode of parent directory
	struct inode *parent_inode = malloc(sizeof(struct inode));
	if(get_node_by_path(parent_directory, 0, parent_inode)==-1){
		free(parent_inode);
		return -1;
	}

	// Step 3: Call get_avail_ino() to get an available inode number
	int empty_ino = get_avail_ino();
	
	// Step 4: Call dir_add() to add directory entry of target directory to parent directory
	dir_add(*parent_inode, empty_ino, target_directory, strlen(target_directory));
	
	// Step 5: Update inode for target directory
	struct inode *target_inode = malloc(sizeof(struct inode));
	target_inode->ino = empty_ino;
	target_inode->valid=1;
	target_inode->size=0;
	target_inode->type=DIR_TYPE;
	target_inode->link=2;				
	for(int x=0; x<16; x++){
		target_inode->direct_ptr[x]=-1;
		target_inode->indirect_ptr[x/2]=-1;
	}
	struct stat* vstat=malloc(sizeof(struct stat));
	vstat->st_mode   = S_IFDIR | 0755;
	time(& vstat->st_mtime);
	target_inode->vstat=*vstat;

	// Step 6: Call writei() to write inode to disk
	writei(empty_ino, target_inode);

	free(target_inode);
	free(parent_inode);

	return 0;
}

static int rufs_rmdir(const char *path) {

	// Step 1: Use dirname() and basename() to separate parent directory path and target directory name
	char *parent_directory = dirname((char*) path);
	char *target_directory = basename((char*) path);

	// Step 2: Call get_node_by_path() to get inode of target directory
	struct inode *target_inode = malloc(sizeof(struct inode));
	if(get_node_by_path(target_directory, 0, target_inode)==-1){
		free(target_inode);
		return -1;
	}

	// Step 3: Clear data block bitmap of target directory
	bitmap_t data_bitmap=malloc(BLOCK_SIZE);
	bio_read(super_block->d_bitmap_blk, data_bitmap);
	for(int x = 0; x<16; x++){
		if(target_inode->direct_ptr[x]>=0 && get_bitmap(data_bitmap, target_inode->direct_ptr[x])){
			unset_bitmap(data_bitmap, target_inode->direct_ptr[x]);
		}
	}
	bio_write(super_block->d_bitmap_blk, data_bitmap);
	free(data_bitmap);

	// Step 4: Clear inode bitmap and its data block
	bitmap_t inode_bitmap=malloc(BLOCK_SIZE);
	bio_read(super_block->i_bitmap_blk, inode_bitmap);
	unset_bitmap(inode_bitmap, target_inode->ino);
	bio_write(super_block->i_bitmap_blk, inode_bitmap);
	free(inode_bitmap);

	// Step 5: Call get_node_by_path() to get inode of parent directory
	struct inode *parent_inode = malloc(sizeof(struct inode));
	if(get_node_by_path(parent_directory, 0, parent_inode)==-1){
		free(parent_inode);
		return -1;
	}

	// Step 6: Call dir_remove() to remove directory entry of target directory in its parent directory
	dir_remove(*parent_inode, target_directory, strlen(target_directory));

	free(target_inode);
	free(parent_inode);
	return 0;
}

static int rufs_releasedir(const char *path, struct fuse_file_info *fi) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
    return 0;
}

static int rufs_create(const char *path, mode_t mode, struct fuse_file_info *fi) {

	// Step 1: Use dirname() and basename() to separate parent directory path and target file name
	char *parent_directory = dirname((char*) path);
	char *target_file = basename((char*) path);

	// Step 2: Call get_node_by_path() to get inode of parent directory
	struct inode *parent_inode = malloc(sizeof(struct inode));
	if(get_node_by_path(parent_directory, 0, parent_inode)==-1){
		free(parent_inode);
		return -1;
	}

	// Step 3: Call get_avail_ino() to get an available inode number
	int empty_ino = get_avail_ino();
	
	// Step 4: Call dir_add() to add directory entry of target file to parent directory
	dir_add(*parent_inode, empty_ino, target_file, strlen(target_file));
	
	// Step 5: Update inode for target file
	struct inode *target_inode = malloc(sizeof(struct inode));
	target_inode->ino = empty_ino;
	target_inode->valid=1;
	target_inode->size=0;
	target_inode->type=FILE_TYPE;
	target_inode->link=1;				
	for(int x=0; x<16; x++){
		target_inode->direct_ptr[x]=-1;
		target_inode->indirect_ptr[x/2]=-1;
	}
	struct stat* vstat=malloc(sizeof(struct stat));
	vstat->st_mode   = S_IFREG | 0666;
	time(& vstat->st_mtime);
	target_inode->vstat=*vstat;

	// Step 6: Call writei() to write inode to disk
	writei(empty_ino, target_inode);

	free(target_inode);
	free(parent_inode);
	return 0;
}

static int rufs_open(const char *path, struct fuse_file_info *fi) {

	// Step 1: Call get_node_by_path() to get inode from path
	struct inode *inode = malloc(sizeof(struct inode));
	if(get_node_by_path(path, 0, inode)==-1){
		// Step 2: If not find, return -1
		free(inode);
		return -1;
	}

	free(inode);
	return 0;
}

static int rufs_read(const char *path, char *buffer, size_t size, off_t offset, struct fuse_file_info *fi) {

	// Step 1: You could call get_node_by_path() to get inode from path
	struct inode *inode = malloc(sizeof(struct inode));
	if(get_node_by_path(path, 0, inode)==-1){
		free(inode);
		return -1;
	}
	int block_offset = offset/BLOCK_SIZE;
	int data_offset = offset%BLOCK_SIZE;
	int data_read = 0;
	int* data_buffer = malloc(BLOCK_SIZE);
	char *end_of_buffer = buffer;
	// Step 2: Based on size and offset, read its data blocks from disk
	for(int x = block_offset; x<16; x++){
		int d_ptr = inode->direct_ptr[x];
		if(d_ptr >= 0){
			int block_number = super_block->d_start_blk;
			block_number += d_ptr;
			if(bio_read(block_number, data_buffer)<=0)
				continue;
			if(data_offset){
				data_buffer+=data_offset;
				if(size>= (BLOCK_SIZE-data_offset)){
					memcpy(end_of_buffer,data_buffer,BLOCK_SIZE-data_offset);
					size -= (BLOCK_SIZE-data_offset);
					data_read += (BLOCK_SIZE-data_offset);
					end_of_buffer += (BLOCK_SIZE-data_offset);
					data_offset = 0;
				}else{
					memcpy(end_of_buffer,data_buffer,size);
					size -= size;
					data_read += size;
					data_offset = 0;
					break;
				}
			}
			else{
				if(size>= BLOCK_SIZE){
					memcpy(end_of_buffer,data_buffer,BLOCK_SIZE);
					size -= BLOCK_SIZE;
					data_read += BLOCK_SIZE;
					end_of_buffer += BLOCK_SIZE;
				}else{
					memcpy(end_of_buffer,data_buffer,size);
					size -= size;
					data_read += size;
					break;
				}
			}

		}
	}


	// Step 3: copy the correct amount of data from offset to buffer
	if(size)
		printf("FROM RUFS_READ: More data needs to be read");
	// Note: this function should return the amount of bytes you copied to buffer
	free(inode);
	free(data_buffer);
	return data_read;
}

static int rufs_write(const char *path, const char *buffer, size_t size, off_t offset, struct fuse_file_info *fi) {

	// Step 2: Based on size and offset, read its data blocks from disk

	// Step 3: Write the correct amount of data from offset to disk

	// Step 1: You could call get_node_by_path() to get inode from path
	struct inode *inode = malloc(sizeof(struct inode));
	if(get_node_by_path(path, 0, inode)==-1){
		free(inode);
		return -1;
	}
	int block_offset = offset/BLOCK_SIZE;
	int data_offset = offset%BLOCK_SIZE;
	int data_write = 0;
	int* data_buffer = malloc(BLOCK_SIZE);
	char *end_of_buffer = (char *)buffer;
	// Step 2: Based on size and offset, read its data blocks from disk
	for(int x = block_offset; x<16; x++){
		int d_ptr = inode->direct_ptr[x];
		int block_number = super_block->d_start_blk;
		if(d_ptr==-1){
			inode->direct_ptr[x] = get_avail_blkno();
			block_number += inode->direct_ptr[x];
		}else if(d_ptr>=0){
			block_number += d_ptr;
			bio_read(block_number, data_buffer);
		}

		if(data_offset){
			int *end_of_data_buffer = data_buffer;
			end_of_data_buffer+=data_offset;
			if(size>= (BLOCK_SIZE-data_offset)){
				memcpy(end_of_data_buffer, end_of_buffer, BLOCK_SIZE-data_offset);
				bio_write(block_number, data_buffer);
				size -= (BLOCK_SIZE-data_offset);
				data_write += (BLOCK_SIZE-data_offset);
				end_of_buffer += (BLOCK_SIZE-data_offset);
				data_offset = 0;
			}else{
				memcpy(end_of_data_buffer, end_of_buffer, size);
				bio_write(block_number, data_buffer);
				size -= size;
				data_write += size;
				data_offset = 0;
				break;
			}
		}else{
			if(size>= BLOCK_SIZE){
				memcpy(data_buffer, end_of_buffer, BLOCK_SIZE);
				bio_write(block_number, data_buffer);
				size -= BLOCK_SIZE;
				data_write += BLOCK_SIZE;
				end_of_buffer += BLOCK_SIZE;
			}else{
				memcpy(data_buffer, end_of_buffer, size);
				bio_write(block_number, data_buffer);
				size -= BLOCK_SIZE;
				data_write += BLOCK_SIZE;
				break;
			}
		}
		
	}
	// Step 4: Update the inode info and write it to disk
	writei(inode->ino, inode);
	free(data_buffer);
	free(inode);

	// Note: this function should return the amount of bytes you write to disk
	return size;
}

static int rufs_unlink(const char *path) {

	// Step 1: Use dirname() and basename() to separate parent directory path and target file name
	char *parent_directory = dirname((char*) path);
	char *target_file = basename((char*) path);

	// Step 2: Call get_node_by_path() to get inode of target file
	struct inode *target_inode = malloc(sizeof(struct inode));
	if(get_node_by_path(target_file, 0, target_inode)==-1){
		free(target_inode);
		return -1;
	}

	// Step 3: Clear data block bitmap of target file
	bitmap_t data_bitmap=malloc(BLOCK_SIZE);
	bio_read(super_block->d_bitmap_blk, data_bitmap);
	for(int x = 0 ; x<16; x++){
		int d_ptr = target_inode->direct_ptr[x];
		if(d_ptr >= 0 ){
			unset_bitmap(data_bitmap, d_ptr-super_block->d_start_blk);
		}
	}
	bio_write(super_block->d_bitmap_blk, data_bitmap);
	free(data_bitmap);

	// Step 4: Clear inode bitmap and its data block
	bitmap_t inode_bitmap=malloc(BLOCK_SIZE);
	bio_read(super_block->i_bitmap_blk, inode_bitmap);
	unset_bitmap(inode_bitmap, target_inode->ino);
	bio_write(super_block->i_bitmap_blk, inode_bitmap);
	free(inode_bitmap);
	free(target_inode);

	// Step 5: Call get_node_by_path() to get inode of parent directory
	struct inode *parent_inode = malloc(sizeof(struct inode));
	if(get_node_by_path(parent_directory, 0, parent_inode)==-1){
		free(parent_inode);
		return -1;
	}
	// Step 6: Call dir_remove() to remove directory entry of target file in its parent directory
	dir_remove(*parent_inode, target_file, strlen(target_file));
	free(parent_inode);
	return 0;
}

static int rufs_truncate(const char *path, off_t size) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
    return 0;
}

static int rufs_release(const char *path, struct fuse_file_info *fi) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
	return 0;
}

static int rufs_flush(const char * path, struct fuse_file_info * fi) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
    return 0;
}

static int rufs_utimens(const char *path, const struct timespec tv[2]) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
    return 0;
}


static struct fuse_operations rufs_ope = {
	.init		= rufs_init,
	.destroy	= rufs_destroy,

	.getattr	= rufs_getattr,
	.readdir	= rufs_readdir,
	.opendir	= rufs_opendir,
	.releasedir	= rufs_releasedir,
	.mkdir		= rufs_mkdir,
	.rmdir		= rufs_rmdir,

	.create		= rufs_create,
	.open		= rufs_open,
	.read 		= rufs_read,
	.write		= rufs_write,
	.unlink		= rufs_unlink,

	.truncate   = rufs_truncate,
	.flush      = rufs_flush,
	.utimens    = rufs_utimens,
	.release	= rufs_release
};


int main(int argc, char *argv[]) {
	int fuse_stat;

	getcwd(diskfile_path, PATH_MAX);
	strcat(diskfile_path, "/DISKFILE");

	fuse_stat = fuse_main(argc, argv, &rufs_ope, NULL);

	return fuse_stat;
}
