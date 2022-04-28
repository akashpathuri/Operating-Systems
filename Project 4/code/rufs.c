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

#include "block.h"
#include "rufs.h"

char diskfile_path[PATH_MAX];

// Declare your in-memory data structures here
const int INODE_BLOCK = 1;
const int DATA_BLOCK = 2;
int disk = -1;

struct superblock* super_block;
int inodes_per_block;
int entries_per_data_block;
/* 
 * Get available inode number from bitmap
 */
int get_avail_ino() {

	// Step 1: Read inode bitmap from disk
	bitmap_t inode_bitmap = malloc(BLOCK_SIZE);
	bio_read(INODE_BLOCK, inode_bitmap);

	// Step 2: Traverse inode bitmap to find an available slot
	for(int i=0; i<MAX_INUM; i++){
		// Step 3: Update inode bitmap and write to disk 	
		if(!get_bitmap(inode_bitmap, i)){
			set_bitmap(inode_bitmap, i);
			bio_write(INODE_BLOCK, inode_bitmap);
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
	bio_read(DATA_BLOCK, data_block_bitmap);

	// Step 2: Traverse data block bitmap to find an available slot
	for(int i=0; i<MAX_DNUM; i++){
		// Step 3: Update data block bitmap and write to disk 	
		if(!get_bitmap(data_block_bitmap, i)){
			set_bitmap(data_block_bitmap, i);
			bio_write(DATA_BLOCK, data_block_bitmap);
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
	struct inode* dir_inode=malloc(sizeof(struct inode));
	int failure=readi(ino,dir_inode);
	if(failure<0){
		perror("Readi failed reading directory inode\n");
		return failure;
	}
  	// Step 2: Get data block of current directory from inode
	int *dir_inode_data_block =dir_inode->direct_ptr;

	// Step 3: Read directory's data block and check each directory entry.
	//If the name matches, then copy directory entry to dirent structure

	
	struct dirent* dirent_data_block=calloc(1,BLOCK_SIZE);
	
	
	//Loop through each dir_entry in each data block to check if name exists 
	for (int data_block_index=0;data_block_index<16;data_block_index++){
		
		if(dir_inode_data_block[data_block_index]==-1){
			//if current block is empty then continue over to the next data block
			continue;
		}

		//read data block containing the dirent entries
		bio_read(super_block->d_start_blk+dir_inode_data_block[data_block_index], dirent_data_block);
		
		//Loop through all the dir entries in this data block
		for(int dirent_index=0;dirent_index<entries_per_data_block;dirent_index++){
			
			struct dirent* dir_entry=dirent_data_block+dirent_index;
		
			if(dir_entry==NULL || dir_entry->valid==0){
				//continue because we are looking for a specific name and one that is valid so there is no need to call strcmp
				continue;
			}
			
			if(strcmp(dir_entry->name,fname)==0){
			
				*dirent=*dir_entry; //point the address of the given dirent to the dir_entry address for the user to use later
				
				free(dir_inode);
				free(dirent_data_block);
				return dir_inode_data_block[data_block_index];
			}
		}
	}
	//Not found
	free(dir_inode);
	free(dirent_data_block);
	return -1;




	return 0;
}

int dir_add(struct inode dir_inode, uint16_t f_ino, const char *fname, size_t name_len) {

	
	// Step 1: Read dir_inode's data block and check each directory entry of dir_inode
	
	int *dir_inode_data=calloc(16,sizeof(int));
	memcpy(dir_inode_data, dir_inode.direct_ptr, 16*sizeof(int));

	// Step 2: Check if fname (directory name) is already used in other entries
	//index of the direct pointer
	
	struct dirent* dirent_data_block=calloc(1,BLOCK_SIZE);
	
	//boolean flag to see if there is an empty dir-entry within a data block
	int found_empty_slot=0;

	//unallocated data block index
	int empty_unallocated_block=-1;
	
	// index of block where there is an empty dir-entry
	int empty_block_index=0;
	// index of empty dir-entry
	int empty_dirent_index=0;

	bitmap_t data_bitmap=malloc(BLOCK_SIZE);
	bio_read(DATA_BLOCK, data_bitmap);
	//Loop through each dir_entry in each data block to check if name exists 
	for (int data_block_index=0;data_block_index<16;data_block_index++){
		
		if(dir_inode_data[data_block_index]==-1){
			
			//If empty, then we set the unallocated block to this index for future reference and skip to the next data block
			if(empty_unallocated_block==-1){
				empty_unallocated_block = data_block_index;
			}
			//continue to next block
			continue;
		}

		//read data block containing dirent entries
		bio_read(super_block->d_start_blk+dir_inode_data[data_block_index],dirent_data_block);

		//Loop through the dirent entries in this block
		for(int dirent_index=0;dirent_index<entries_per_data_block;dirent_index++){
			struct dirent* dir_entry=dirent_data_block+dirent_index;
			
			//if the directory entry is either NULL or invalid, it can be overwritten
			if(dir_entry==NULL || dir_entry->valid==0){
				
				//found an empty slot in an allocated block. Takes priority over unallocated block.
				if(found_empty_slot==0){
					found_empty_slot=1;
					empty_block_index=data_block_index;
					empty_dirent_index=dirent_index;
				}
				//continue because we don't care about null or invalid entries' names
				continue;
			}
			
			if(strcmp(dir_entry->name,fname)==0){
			
				return -1; //if name exists then exit function
			}
			// else{
			// 	//printf("%s doesn't equal %s\n",dir_entry->name,fname);
			// }
		}
	}

	// Step 3: Add directory entry in dir_inode's data block and write to disk
	
	struct dirent* new_entry=malloc(sizeof(struct dirent));
	
	new_entry->valid=1;

	new_entry->ino=f_ino;

	strncpy(new_entry->name,fname,name_len+1);
	
	//if from previous iterations we have found an empty slot already then we set the entry otherwise we search
	if(found_empty_slot==1){
		//printf("found an empty slot in allocated data block: %d at entry # %d\n",dir_inode_data[empty_block_index],empty_dirent_index);
		
		bio_read(super_block->d_start_blk+dir_inode_data[empty_block_index],dirent_data_block);
		dirent_data_block[empty_dirent_index]=*new_entry;
		bio_write(super_block->d_start_blk+dir_inode_data[empty_block_index],dirent_data_block);
		free(new_entry);

	}
	// Allocate a new data block for this directory if it does not exist
	else if(empty_unallocated_block>-1){
		//confirming the index is empty
		if(dir_inode_data[empty_unallocated_block]==-1){//unallocated block
			
			//get next available block num and set the bit to 1
			int block_num = get_avail_blkno();// get_avail_blkno will set the data_bitmap to 1 for this block
			
			//make the inode's direct pointer point to this data block
			dir_inode_data[empty_unallocated_block]=block_num;
			
			//printf("set pointer num %d to data block %d\n",empty_unallocated_block,block_num);
			
			//malloc space for the new block
			struct dirent* new_data_block=malloc(BLOCK_SIZE);
			
			//set the first entry of the new data block to store the new entry
			//*new_data_block=*new_entry;
			new_data_block[0]=*new_entry;
			
			//write to disk
			bio_write(super_block->d_start_blk+block_num,new_data_block);
		
			//printf("wrote to block %d\n",SB->d_start_blk+block_num);
			free(new_data_block);
			free(new_entry);
		}
		else{
			printf("We done messed up.\n");
			free(new_entry);
		}
	}
	else{
		//No free data blocks in this directory
		printf("No free data blocks in directory.\n");
		//remove the bit for the inode that was passed in since we aren't creating it.
		bitmap_t inode_bitmap=malloc(BLOCK_SIZE);
		//update the bitmap
		bio_read(INODE_BLOCK, inode_bitmap);
		unset_bitmap(inode_bitmap,f_ino);
		bio_write(INODE_BLOCK, inode_bitmap);
		free(inode_bitmap);
		free(data_bitmap);
		free(dirent_data_block);
		free(dir_inode_data);
		return -1;
	}
	// Update directory inode

	//update parent inode
	int parent_ino=dir_inode.ino;
	struct inode* parent_inode=malloc(sizeof(struct inode));
	readi(parent_ino,parent_inode);
	//update size of parent directory
	parent_inode->size+=sizeof(struct dirent);

	//update the direct_ptrs
	//parent_inode->direct_ptr=dir_inode_data
	memcpy(parent_inode->direct_ptr,dir_inode_data,16*sizeof(int));
	//update access time of parent
	time(& (parent_inode->vstat.st_mtime));
	// Write directory entry
	writei(parent_ino,parent_inode);

	free(parent_inode);
	free(data_bitmap);
	free(dirent_data_block);
	free(dir_inode_data);
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

	// write superblock information

	// initialize inode bitmap

	// initialize data block bitmap

	// update bitmap information for root directory

	// update inode for root directory

	return 0;
}


/* 
 * FUSE file operations
 */
static void *rufs_init(struct fuse_conn_info *conn) {
	//initiliaze global variables
	inodes_per_block=BLOCK_SIZE/sizeof(struct inode);
	entries_per_data_block=BLOCK_SIZE/sizeof(struct dirent);

	// Step 1a: If disk file is not found, call mkfs
	disk=dev_open(diskfile_path);
	if(disk<0){
		if(rufs_mkfs()<0){ 
			printf("error making disk\n");
		}
	}
  // Step 1b: If disk file is found, just initialize in-memory data structures
  // and read superblock from disk
	else
		{
			//printf("Disk has already been created and is %d\n",disk);
			super_block=(struct superblock*) malloc(BLOCK_SIZE);
			bio_read(0,super_block);

			if(super_block->magic_num!=MAGIC_NUM){
				//TODO: something when disk isn't ours
				printf("magic nums don't match\n");
				exit(-1);
			}

	}


	return NULL;
}

static void rufs_destroy(void *userdata) {

	// Step 1: De-allocate in-memory data structures

	// Step 2: Close diskfile

}

static int rufs_getattr(const char *path, struct stat *stbuf) {

	// Step 1: call get_node_by_path() to get inode from path

	// Step 2: fill attribute of file into stbuf from inode

		stbuf->st_mode   = S_IFDIR | 0755;
		stbuf->st_nlink  = 2;
		time(&stbuf->st_mtime);

	return 0;
}

static int rufs_opendir(const char *path, struct fuse_file_info *fi) {

	// Step 1: Call get_node_by_path() to get inode from path

	// Step 2: If not find, return -1

    return 0;
}

static int rufs_readdir(const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {

	// Step 1: Call get_node_by_path() to get inode from path

	// Step 2: Read directory entries from its data blocks, and copy them to filler

	return 0;
}


static int rufs_mkdir(const char *path, mode_t mode) {

	// Step 1: Use dirname() and basename() to separate parent directory path and target directory name

	// Step 2: Call get_node_by_path() to get inode of parent directory

	// Step 3: Call get_avail_ino() to get an available inode number

	// Step 4: Call dir_add() to add directory entry of target directory to parent directory

	// Step 5: Update inode for target directory

	// Step 6: Call writei() to write inode to disk
	

	return 0;
}

static int rufs_rmdir(const char *path) {

	// Step 1: Use dirname() and basename() to separate parent directory path and target directory name

	// Step 2: Call get_node_by_path() to get inode of target directory

	// Step 3: Clear data block bitmap of target directory

	// Step 4: Clear inode bitmap and its data block

	// Step 5: Call get_node_by_path() to get inode of parent directory

	// Step 6: Call dir_remove() to remove directory entry of target directory in its parent directory

	return 0;
}

static int rufs_releasedir(const char *path, struct fuse_file_info *fi) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
    return 0;
}

static int rufs_create(const char *path, mode_t mode, struct fuse_file_info *fi) {

	// Step 1: Use dirname() and basename() to separate parent directory path and target file name

	// Step 2: Call get_node_by_path() to get inode of parent directory

	// Step 3: Call get_avail_ino() to get an available inode number

	// Step 4: Call dir_add() to add directory entry of target file to parent directory

	// Step 5: Update inode for target file

	// Step 6: Call writei() to write inode to disk

	return 0;
}

static int rufs_open(const char *path, struct fuse_file_info *fi) {

	// Step 1: Call get_node_by_path() to get inode from path

	// Step 2: If not find, return -1

	return 0;
}

static int rufs_read(const char *path, char *buffer, size_t size, off_t offset, struct fuse_file_info *fi) {

	// Step 1: You could call get_node_by_path() to get inode from path

	// Step 2: Based on size and offset, read its data blocks from disk

	// Step 3: copy the correct amount of data from offset to buffer

	// Note: this function should return the amount of bytes you copied to buffer
	return 0;
}

static int rufs_write(const char *path, const char *buffer, size_t size, off_t offset, struct fuse_file_info *fi) {
	// Step 1: You could call get_node_by_path() to get inode from path

	// Step 2: Based on size and offset, read its data blocks from disk

	// Step 3: Write the correct amount of data from offset to disk

	// Step 4: Update the inode info and write it to disk

	// Note: this function should return the amount of bytes you write to disk
	return size;
}

static int rufs_unlink(const char *path) {

	// Step 1: Use dirname() and basename() to separate parent directory path and target file name

	// Step 2: Call get_node_by_path() to get inode of target file

	// Step 3: Clear data block bitmap of target file

	// Step 4: Clear inode bitmap and its data block

	// Step 5: Call get_node_by_path() to get inode of parent directory

	// Step 6: Call dir_remove() to remove directory entry of target file in its parent directory

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
