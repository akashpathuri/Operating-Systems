/*
  Copyright (C) 2012 Joseph J. Pfeiffer, Jr., Ph.D. <pfeiffer@cs.nmsu.edu>
  This program can be distributed under the terms of the GNU GPLv3.
  See the file COPYING.
  There are a couple of symbols that need to be #defined before
  #including all the headers.
*/

#ifndef _PARAMS_H_
#define _PARAMS_H_

// The FUSE API has been changed a number of times.  So, our code
// needs to define the version of the API that we assume.  As of this
// writing, the most current API version is 26
#define FUSE_USE_VERSION 26

// need this to get pwrite().  I have to use setvbuf() instead of
// setlinebuf() later in consequence.
#define _XOPEN_SOURCE 500

// maintain bbfs state in here
#include <limits.h>
#include <stdio.h>
#include <sys/stat.h>
#include "block.h"

#define TOTAL_BLOCKS 1024
#define TOTAL_INODE_NUMBER ((BLOCK_SIZE*64)/(sizeof(struct inode_)))
#define TOTAL_DATA_BLOCKS (TOTAL_BLOCKS - TOTAL_INODE_NUMBER - 1)
#define TYPE_DIRECTORY 0
#define TYPE_FILE 1
#define TYPE_LINK 2 

//typedef struct inode inode_t;
typedef struct file_descriptor fd_t;
/* data structures
 * super block to hold the info of data file
 * inode to hold the info of a file
 * bitmaps to notify how are inodes and data blocks being used
 */

struct superblock
{
  int inodes;
  int fs_type;
  int data_blocks;
  int i_list;
};


typedef struct inode_
{//256 bytes now...
  int id;
  int size;
  int uid;
  int gid;
  int type; 
  int links;
  int blocks;
  mode_t st_mode; //32 bytes 
  unsigned char path[64]; //96 bytes
  unsigned int data_blocks[15];//156 bytes
  time_t last_accessed, created, modified;// 180 bytes
  int data_blocks_level;  //to implement data block indirect.. always indirect at the last data_block; 
  char unusedspace[64];
}inode;


struct i_list{ //use a struct to make it in the heap
  inode table[TOTAL_INODE_NUMBER];
};


struct i_bitmap
{
  unsigned char bitmap[TOTAL_INODE_NUMBER/8];
  int size;
};

struct block_bitmap
{
  unsigned char bitmap[TOTAL_DATA_BLOCKS/8];
  int size;
};

struct file_descriptor{
  int id;
  int inode_id;
};

struct fd_table{
  fd_t table[TOTAL_INODE_NUMBER];
};


struct superblock supablock;
struct i_bitmap inodes_bm;
struct block_bitmap block_bm;
struct i_list inodes_table;
struct fd_table fd;



struct sfs_state {
    FILE *logfile;
    char *diskfile;
};

#define SFS_DATA ((struct sfs_state *) fuse_get_context()->private_data)

#endif
