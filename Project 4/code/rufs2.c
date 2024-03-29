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
#define DIRECT_SIZE 1024
#define FILE_SIZE 256
#define DIRENT_SIZE 512
	

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
	block += offset;
	memcpy(inode, block, sizeof(struct inode));
	free(block);
	return 1;
}

int writei(uint16_t ino, struct inode *inode) {

	// Step 1: Get the block number where this inode resides on disk
	int block_number = super_block->i_start_blk;
	block_number+= ino/inodes_per_block;

	struct inode *block = malloc(BLOCK_SIZE);
	if(bio_read(block_number, block)<0)
		return 0;

	// Step 2: Get the offset in the block where this inode resides on disk
	int block_offset = ino%inodes_per_block;
	//block_offset *= sizeof(struct inode);

	// Step 3: Write inode to disk 
	int offset = block_offset * sizeof(struct inode);
	struct inode* addrOfInode=(struct inode*) (block+offset); // make sure the cast doesn't make things break
	//*addrOfInode=*inode;
	//block[block_offset] = *inode;
	memcpy(addrOfInode, inode, sizeof(struct inode));
	printf("writing inode in %d \n", block_number);
	// struct inode *inode_entry = (struct inode *)block;
	// for(int y = 0; y<dirent_per_block; y++){
	// 	inode_entry = (struct inode *)inode_entry+y;
	// 	if(inode_entry ==NULL || !inode_entry->valid ){
	// 		memcpy(inode_entry, inode, sizeof(struct inode));
	// 		//inode_entry = inode;
	// 		//bio_write(block_number, block);
	// 		break;
	// 	}
	// }



	// struct inode* end_of_block=(struct inode*) (block); // make sure the cast doesn't make things break
	// for(int x = 0; x<inodes_per_block; x++){
	// 	if(x == block_offset){
	// 		//addrOfInode = inode;
	// 		printf("writing inode to block at offset %d\n", block_offset);

	// 	}else{
	// 		printf("writing inode to block at offset -1\n");
	// 	}
	// 	end_of_block += sizeof(struct inode);
		
	// }
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
	printf("finding ino %d\n", ino);
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
		int block_number = super_block->d_start_blk;
		if(d_ptr >= 0){
			printf("Looping directory block %d\n", block_number+d_ptr);
			block_number += d_ptr;
			if(bio_read(block_number, data_buffer)<=0)
				continue;
			for(int y = 0; y<dirent_per_block; y++){
				struct dirent *directory_entry = (struct dirent *)data_buffer+y;
				if(directory_entry!=NULL && directory_entry->valid &&!strcmp(directory_entry->name, fname)){
					printf("found dir with name %s\n", directory_entry->name);
					dirent=directory_entry;
					free(directory_inode);
					free(data_buffer);
					return block_number;
				}
			}
		}else{
			printf("Looping directory block %d\n", d_ptr);
		}
	}
	free(directory_inode);
	free(data_buffer);
	return -1;
}

int dir_add(struct inode dir_inode, uint16_t f_ino, const char *fname, size_t name_len) {

	// Step 1: Read dir_inode's data block and check each directory entry of dir_inode
	// Step 2: Check if fname (directory name) is already used in other entries
	struct dirent* directory_exits=malloc(sizeof(struct dirent));
	if(dir_find(dir_inode.ino, fname, name_len, directory_exits)>=0){
		free(directory_exits);
		return -1;
	}
	free(directory_exits);

	// Step 3: Add directory entry in dir_inode's data block and write to disk
	printf("Adding New Directory");
	struct dirent* new_directory=malloc(sizeof(struct dirent));
	new_directory->valid=1;
	new_directory->ino=f_ino;
	strncpy(new_directory->name,fname,name_len+1);

	struct dirent *data_buffer = malloc(BLOCK_SIZE);
	for(int x = 0; x<16; x++){
		int d_ptr = dir_inode.direct_ptr[x];
		if(d_ptr >= 0){
			int block_number = super_block->d_start_blk;
			block_number += d_ptr;
			if(bio_read(block_number, data_buffer)<=0)
				continue;
			for(int y = 0; y<dirent_per_block; y++){
				struct dirent *directory_entry = (struct dirent *)data_buffer+y;
				if(directory_entry ==NULL || !directory_entry->valid ){
					memcpy(directory_entry, new_directory, sizeof(struct dirent));
					//directory_entry = new_directory;
					bio_write(block_number, data_buffer);
					break;
				}
			}
		}else{
			dir_inode.direct_ptr[x] = get_avail_blkno();
			printf("creating new dirent Block at %d with %d\n", x,dir_inode.direct_ptr[x]);

			struct dirent* new_dirent_block=malloc(BLOCK_SIZE);
			new_dirent_block[0]=*new_directory;
			//write to disk
			int block_number = super_block->d_start_blk;
			block_number += dir_inode.direct_ptr[x];
			printf("writing at block %d with %d\n", block_number, dir_inode.direct_ptr[x]);
			bio_write(block_number,new_dirent_block);

			break;
		}
	}
	free(data_buffer);
	// Allocate a new data block for this directory if it does not exist

	// Update directory inode
	int parent_ino=dir_inode.ino;
	struct inode* parent_inode=malloc(sizeof(struct inode));
	readi(parent_ino,parent_inode);

	parent_inode->size+=sizeof(struct dirent);
	memcpy(parent_inode->direct_ptr,dir_inode.direct_ptr,16*sizeof(int));
	time(& (parent_inode->vstat.st_mtime));

	// Write directory entry
	// writei(dir_inode.ino, &dir_inode);
	printf("root inode pointer %d\n", parent_inode->direct_ptr[0]);
	writei(parent_ino,parent_inode);

	return 0;
}

int dir_remove(struct inode dir_inode, const char *fname, size_t name_len) {

	// Step 1: Read dir_inode's data block and checks each directory entry of dir_inode
	
	// Step 2: Check if fname exist

	// Step 3: If exist, then remove it from dir_inode's data block and write to disk
	// Step 1: Read dir_inode's data block and checks each directory entry of dir_inode
	uint16_t parent_ino=dir_inode.ino;
	struct dirent *dirent_to_remove=calloc(1, sizeof(struct dirent));
	// Step 2: Check if fname exist
	int data_block_num=dir_find(parent_ino, fname, name_len, dirent_to_remove);
	if(data_block_num<0 || dirent_to_remove->valid==0){
		printf("%s is not in the directory, cannot remove.\n",fname);
		return data_block_num;
	}
	printf("data block of dir entry to remove is %d\n",data_block_num);
	// Step 3: If exist, then remove it from dir_inode's data block (set valid to 0) and write to disk
	//first set te dirent's valid bit to 0
	struct dirent* data_block=malloc(BLOCK_SIZE);
	// read block
	bio_read(super_block->d_start_blk+data_block_num,data_block);
	int i=0;
	int num_valid_entries=0;
	for(i=0;i<dirent_per_block;i++){
		struct dirent* dir_entry=data_block+i;
		//printf("checking inode # %d\n",dir_entry->ino);

		//remove directory entry by setting it's valid bit to 0
		if(dir_entry->ino==dirent_to_remove->ino){
			dir_entry->valid=0;

			printf("removed ino %d\n",dir_entry->ino);
		}
		else if(dir_entry->valid==1){
				num_valid_entries+=1;
		}

	}
	//commit changes to disk
	bio_write(super_block->d_start_blk+data_block_num,data_block);

	free(data_block);
	printf("number of valid entries after removing %d\n",num_valid_entries);
	if(num_valid_entries==0){
		//no valid entries left in the data block. Remove the data block.
		for(i=0;i<16;i++){
			printf("comparing direct pointer %d to %d with block to remove: %d\n",i,dir_inode.direct_ptr[i],data_block_num);
			//search for the pointer to the data block, so we can remove it
			if(dir_inode.direct_ptr[i]==data_block_num){
				printf("data block %d is now empty. Removing.\n",dir_inode.direct_ptr[i]);
				struct inode* parent_inode=malloc(sizeof(struct inode));
				readi(parent_ino,parent_inode);
				parent_inode->direct_ptr[i]=-1;
				writei(parent_ino,parent_inode);
				printf("set direct pointer index %d to -1\n",i );
				free(parent_inode);

				//clear the data bitmap since we removed this data block
				bitmap_t data_bitmap=malloc(BLOCK_SIZE);
				bio_read(super_block->d_bitmap_blk,data_bitmap);
				unset_bitmap(data_bitmap,parent_ino);
				bio_write(super_block->d_bitmap_blk,data_bitmap);
				free(data_bitmap);
				break;
			}
		}
	}


	//If you remove an entire data block, set it's bit to 0 and
	//the direct pointer to -1 !!!
	printf("removed succefully\n");
	return 0;
	//return 0;
}

/* 
 * namei operation
 */
int get_directory_and_file_name(char *path, char* dir_name, char* file_name){
	int i=0;
	int len=strlen(path);
	if(path[0]=='/'){
		path=path+1;
		len--;
	}
	for(i=len-1;i>=0;i--){
		if(path[i]=='/' && path[i+1]!='\0' && len-i-1>0){
			if(*(path+len-1)=='/'){
				// printf("last char was a slash at index %d removing it.\n",path+len-1);
				strncpy(file_name,path+i+1,len-i-2);
				// printf("file name is %s and has length %d\n",file_name, strlen(file_name));

			}
			else{
				// printf("last char is: %c\n",*(path+i+len-i-1));
				strncpy(file_name,path+i+1,len-i-1);
				// printf("file name is %s and has length %d\n",file_name, strlen(file_name));

			}

			// if(file_name[strlen(file_name)-1]=='/'){
			// 	printf("last char was a slash. removing it.\n");
			// 	file_name[len-i-2]='\0';
			// }
			strncpy(dir_name,path,i);
			// /root/abc/a.txt
			return 0;
		}
	}
	// printf("path only specifies file name\n");
	strncpy(file_name,path,strlen(path));
	return -1;
}

void split_path_name(char* dir_path, char* first_dir, char* remaining){
	int i=0;
	int len=strlen(dir_path);
	if(dir_path[0]=='/'){
		dir_path=dir_path+1;
	}
	for(i=0;i<len;i++){
		if(dir_path[i]=='/'){
			strncpy(first_dir,dir_path,i);
			strncpy(remaining,dir_path+i+1,len-i-1);
			return;
		}
	}
	return;
}

int get_node_by_path(const char *path, uint16_t ino, struct inode *inode) {
	
	// Step 1: Resolve the path name, walk through path, and finally, find its inode.
	// if(strcmp(path, "/") == 0){ 
	// 	return 0;     
	// }
		// Step 1: Resolve the path name, walk through path, and finally, find its inode.
	// Note: You could either implement it in a iterative way or recursive way
	//Make sure to calloc so null termnator is included

	if(strcmp(path,"/")==0 || strlen(path)==0){
		printf("Root node is the only one requested.\n"); //print statement change
		//read root into inode
		readi(0,inode);
		return 0;
	}
	char* dir_path_name=calloc(DIRECT_SIZE,sizeof(char));//1024 slight name change
	char* file_path_name=calloc(FILE_SIZE,sizeof(char)); //256 slight name change
	dir_path_name = dirname((char*) path); //different than get_directory_and_file_name()
	file_path_name = basename((char*) path);
	//get_directory_and_file_name((char*)path,dir_path_name,file_path_name);
	
	printf("dir path is: %s and file is: %s\n",dir_path_name,file_path_name);
	char* first_dir=calloc(DIRENT_SIZE,sizeof(char)); //512
	char* remaining=calloc(DIRECT_SIZE,sizeof(char)); //1024 (slight changes, created constants at the top of rufs2.c)
	//int has_slash=0;
	uint16_t prev_ino=0;//name change
	struct dirent* dir=calloc(1,sizeof(struct dirent));
	
	if(strlen(dir_path_name)==0){
		printf("path is root.\n");
		has_slash=-1;
	}


	while(strlen(dir_path_name)>0){ //major change here (possibly wrong)

		split_path_name(dir_path_name,first_dir,remaining); //changed to void function
		//helper method to separate first directory and the remaining directories to traverse, dir_path_name stores all directories
		//first_dir after method call will hold the first directory to handle and future ones as well
		//remaining are the characters remaining after the first directory substring 
		
		if(strlen(dir_path_name)==0){ //changed has_slash to strlen(dir-path-name) ==> Has_slash only is -1 when dir_path_name has length of 0 (possibly wrong need test)
			int find_status=dir_find(prev_ino, dir_path_name, strlen(dir_path_name)+1, dir);
			if(find_status<0){
				printf("failed to find node\n");
				free(dir_path_name);
				free(file_path_name);
				free(first_dir);
				free(remaining);
				free(dir);
				return find_status;
			}
			
			prev_ino=dir->ino;
			memset(dir_path_name, 0, DIRECT_SIZE); //1024
			strcpy(dir_path_name,remaining);
			memset(remaining, 0, DIRECT_SIZE); //1024
			memset(first_dir,0,DIRENT_SIZE); //512
			break;
		}
		//printf("split of dir path is: %s and file is: %s\n",first_dir,remaining);
		int find_status=dir_find(prev_ino, first_dir, strlen(first_dir)+1, dir);
		if(find_status<0){
			printf("Failed to find node\n");
			free(dir_path_name);
			free(file_path_name);
			free(first_dir);
			free(remaining);
			free(dir);
			return find_status;
		}
		prev_ino=dir->ino;
		memset(dir_path_name, 0, DIRECT_SIZE); //1024
		strcpy(dir_path_name,remaining);
		memset(remaining, 0, DIRECT_SIZE);//1024
		memset(first_dir,0,DIRENT_SIZE); //512

	}
	int find_status=dir_find(prev_ino, file_path_name, strlen(file_path_name)+1, dir);
	if(find_status<0){
		printf("dir_find failed\n");
		free(dir_path_name);
		free(file_path_name);
		free(first_dir);
		free(remaining);
		free(dir);
		return find_status;
	}
	struct inode* return_node=calloc(1,sizeof(struct inode));
	int read_status=readi(dir->ino,return_node);
	if(read_status<0)
		{
			printf("read failed.\n");
			free(dir_path_name);
			free(file_path_name);
			free(first_dir);
			free(remaining);
			free(dir);
			return read_status;
		}
	// printf("&&&& found inode %d\n",return_node->ino);
	memcpy(inode,return_node,sizeof(struct inode));
	free(dir_path_name);
	free(file_path_name);
	free(first_dir);
	free(remaining);
	free(dir);
	return read_status;
}


/* 
 * Make file system
 */
int rufs_mkfs() {
	printf("RUFS mkfs\n");
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
	struct inode* root = malloc(sizeof(struct inode));
	root->ino = disk_block;
	root->valid=1;
	root->size=0;
	root->type=DIR_TYPE;
	root->link=2;				
	for(int x=0; x<16; x++){
		root->direct_ptr[x]=-1;
		root->indirect_ptr[x/2]=-1;
	}
	struct stat* vstat=malloc(sizeof(struct stat));
	vstat->st_mode   = S_IFDIR | 0755;
	time(& vstat->st_mtime);
	root->vstat=*vstat;;
	writei(disk_block,root);

	return 0;
}


/* 
 * FUSE file operations
 */
static void *rufs_init(struct fuse_conn_info *conn) {
	printf("RUFS init\n");
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
	printf("RUFS init finished\n");
	return NULL;
}

static void rufs_destroy(void *userdata) {
	printf("RUFS destroy\n");

	// Step 1: De-allocate in-memory data structures
	free(super_block);

	// Step 2: Close diskfile
	dev_close();

}

static int rufs_getattr(const char *path, struct stat *stbuf) {
	printf("RUFS getattr\n");

	// Step 1: call get_node_by_path() to get inode from path
	struct inode *inode = malloc(sizeof(struct inode));
	if(get_node_by_path(path,0,inode)<0){
		printf("Can't get attr of %s since it doesn't exist.\n",path);
		return -ENOENT;
	}
	// Step 2: fill attribute of file into stbuf from inode
	stbuf->st_mode=inode->vstat.st_mode;
	stbuf->st_nlink=inode->link;
	stbuf->st_uid=getuid();
	stbuf->st_gid=getgid();
	stbuf->st_size=inode->size;
	stbuf->st_ino=inode->ino;
	time(&stbuf->st_mtime);

	free(inode);
	return 0;
}

static int rufs_opendir(const char *path, struct fuse_file_info *fi) {
	printf("RUFS opendir\n");

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
	printf("RUFS readdir\n");

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
		int block_number = super_block->d_start_blk;
		if(d_ptr >= 0){
			block_number += d_ptr;
			bio_read(block_number, data_buffer);
			
			for(int y = 0; y<dirent_per_block; y++){
				if(data_buffer[y].valid && filler(buffer, data_buffer[y].name,NULL,offset))
					break;
			}

		}
	}
	free(inode);
	free(data_buffer);
	return 0;
}

static int rufs_mkdir(const char *path, mode_t mode) {

	// Step 1: Use dirname() and basename() to separate parent directory path and target directory name
	printf("RUFS mkdir\n");
	char new_path[100];
	strcpy(new_path, path);
	char *parent_directory = dirname((char*) path);
	char *target_directory = basename((char*) new_path);
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
	printf("RUFS rmdir\n");

	// Step 1: Use dirname() and basename() to separate parent directory path and target directory name
	char new_path[100];
	strcpy(new_path, path);
	char *parent_directory = dirname((char*) path);
	char *target_directory = basename((char*) new_path);


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
	printf("RUFS create %s\n", path);
	char new_path[100];
	strcpy(new_path, path);
	char *parent_directory = dirname((char*) path);
	char *target_file = basename((char*) new_path);

	printf("directory: %s, new target: %s, target: %s\n", parent_directory, new_path, target_file);

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
	printf("directory added \n");
	
	// Step 5: Update inode for target file
	struct inode *target_inode = malloc(sizeof(struct inode));
	target_inode->ino = empty_ino;
	target_inode->valid=1;
	target_inode->size=0;
	target_inode->type=FILE_TYPE;
	target_inode->link=1;				
	for(int x=0; x<16; x++){
		target_inode->direct_ptr[x]=-1;
	}
	struct stat* vstat=malloc(sizeof(struct stat));
	vstat->st_mode   = S_IFREG | 0666;
	time(& vstat->st_mtime);
	target_inode->vstat=*vstat;

	// Step 6: Call writei() to write inode to disk
	printf("writing Inode to %d\n", empty_ino);
	struct dirent* new_dirent_block=malloc(BLOCK_SIZE); 
	dir_find(0, target_file, strlen(target_file),new_dirent_block);
	free(new_dirent_block);
	writei(empty_ino, target_inode);
	struct dirent* new_dirent_block2=malloc(BLOCK_SIZE); 
	dir_find(0, target_file, strlen(target_file),new_dirent_block2);
	free(new_dirent_block2);

	free(target_inode);
	free(parent_inode);
	return 0;
}

static int rufs_open(const char *path, struct fuse_file_info *fi) {
	printf("RUFS Open\n");
	
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
	printf("RUFS read\n");

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
	printf("RUFS write %s\n", path);

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
	printf("RUFS unlink\n");

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
