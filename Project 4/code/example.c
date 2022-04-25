/*
  Simple File System
  This code is derived from function prototypes found /usr/include/fuse/fuse.h
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>
  His code is licensed under the LGPLv2.
*/

#include "params.h"
#include "block.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fuse.h>
#include <libgen.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>

#ifdef HAVE_SYS_XATTR_H
#include <sys/xattr.h>
#endif
 

#include "log.h"



/*  some helper functions */

int find_empty_inode_bit()
{
  int i =0;
  for(;i<inodes_bm.size;i++)
  {
    if(((inodes_bm.bitmap[i / 8] >> (i % 8)) & 1) == 0)
    {
      return i;
    }
  }
  return -1;
}

int find_empty_data_bit()
{
  int i =0;
  for(;i<block_bm.size;i++)
  {
    if(((block_bm.bitmap[i / 8] >> (i % 8)) & 1) == 0)
    {
      return i;
    }
  }
  return -1;
}

void set_inode_bit(int index, int bit)
{
  if(!(bit==0 || bit == 1))
  {
	  return;
  }
  if(bit == 1)
  {
	  inodes_bm.bitmap[index / 8] |= 1 << (index % 8);
  }
  else
  {
	  inodes_bm.bitmap[index / 8] &= ~(1 << (index % 8));
  }
}

void set_block_bit(int index, int bit)
{
  if(!(bit==0 || bit == 1))
  {
	return;
  }
  if(bit == 1)
  {
	block_bm.bitmap[index / 8] |= 1 << (index % 8);
  }
  else
  {
	 block_bm.bitmap[index / 8] &= ~(1 << (index % 8));
  }
}

int get_inode_from_path(const char* path)
{
    int i;
    for(i = 0;i<TOTAL_INODE_NUMBER;i++)
    {
      if(strcmp(inodes_table.table[i].path, path) == 0)
	  {
        return i;
      }
    }
    return -1;
}

int write_inode_to_disk(int index)
{
  int rtn = -1;
  struct inode_ *ptr = &inodes_table.table[index];
  uint8_t *buf = malloc(BLOCK_SIZE*sizeof(uint8_t));
  if(block_read(3+((ptr->id)/2), buf)>-1)  //e.g. inode 0 and 1 should be in block 0+2
  {
      int offset = (ptr->id%(BLOCK_SIZE/sizeof(struct inode_)))*sizeof(struct inode_);
      memcpy(buf+offset, ptr, sizeof(struct inode_));
      if(block_write(3+((ptr->id)/2), buf)>0)
	  {
        rtn = ptr->id;
      }
      else
	  { 
        rtn = -1;
      }      
  }
  free(buf);
  return rtn;
}

int get_empty_fd()
{
  int i;
  for(i = 0; i < TOTAL_INODE_NUMBER; i++)
  {
    if(fd.table[i].inode_id == -1)
	{
		return i;
	}
  }
  return -1;
}

int find_fd(int index)
{
  int i;
  for(i = 0; i < TOTAL_INODE_NUMBER; i++)
  {
    if(fd.table[i].inode_id == index)
	{
		return i;
	}
  }
  return -1;
}

int take_fd(int index, int inode_id)
{
  if(fd.table[index].inode_id == -1)
  {
    fd.table[index].inode_id = inode_id;
    return 0;
  }
  return -1;
}

int check_parent_dir(const char* path, int i)
{
  char *temp = malloc(64);
  int len = strlen(inodes_table.table[i].path);
  memcpy(temp,inodes_table.table[i].path, len);
  *(temp+len) = '\0';
  
  int offset;
  for(offset = len-1; offset>=0 ; offset--)
  {
    if(*(temp+offset) == '/' && offset!=0)
    {
      *(temp+offset)='\0';
      break;
    }
     if(*(temp+offset) == '/'){
      *(temp+offset+1) = '\0';
      break;
    }
  }
  

  if(strcmp(temp, path)== 0)
  {
    free(temp);
    return 0;
  }

  free(temp);
  return -1;
}

char* get_file_name(int i)
{
  int len = strlen(inodes_table.table[i].path);
  char *temp =inodes_table.table[i].path;
  int offset;
  for(offset = len-1; offset>=0 ; offset--)
  {
    if(*(temp+offset) == '/')
    {
      break;
    }
  }
  char *rtn = malloc(len-offset);
  memcpy(rtn, temp+offset+1, len-offset);
  *(rtn+strlen(rtn)+1)='\0';
  return rtn;
}





/* 
 * A function initiate the inodes for the first setup of the file system
 */

void init_data_structure()
{
  int i;
  for(i = 0; i<TOTAL_INODE_NUMBER; i++)
  {
    inodes_table.table[i].id = i;
    int j;
    for(j = 0; j<15;j++)
    {
      inodes_table.table[i].data_blocks[j] = -1;
    }
    memset(inodes_table.table[i].path, 0, 64*sizeof(char)) ;
    inodes_table.table[i].data_blocks_level =0;
  }

  
  memset(inodes_bm.bitmap,0,TOTAL_INODE_NUMBER/8);
  memset(block_bm.bitmap, 0, TOTAL_DATA_BLOCKS/8);

  inodes_bm.size = TOTAL_INODE_NUMBER;
  block_bm.size = TOTAL_DATA_BLOCKS;
  
}

// Prototypes for all these functions, and the C-style comments,
// come indirectly from /usr/include/fuse.h
//

/**
 * Initialize filesystem
 *
 * The return value will passed in the private_data field of
 * fuse_context to all file operations and as a parameter to the
 * destroy() method.
 *
 * Introduced in version 2.3
 * Changed in version 2.6
 */
void *sfs_init(struct fuse_conn_info *conn)
{
    fprintf(stderr, "in bb-init\n");
	log_msg("\nsfs_init()\n");
   
    struct stat *statbuf = (struct stat*) malloc(sizeof(struct stat));
    int in = lstat((SFS_DATA)->diskfile,statbuf);
    
    log_stat(statbuf);

    
    if(in != 0) 
	{
        perror("No STAT on diskfile");
        exit(EXIT_FAILURE);
    }


    disk_open((SFS_DATA)->diskfile);

    in = 0;
    for(; in<TOTAL_INODE_NUMBER;in++)
    {
      fd.table[in].id = in;
      fd.table[in].inode_id = -1;
    }
    
    char *buf = (char*) malloc(BLOCK_SIZE);
    if(block_read(0, buf) <= 0) {
      // initialize superblock etc here in file
      supablock.inodes = TOTAL_INODE_NUMBER;
      supablock.fs_type = 0;
      supablock.data_blocks = TOTAL_DATA_BLOCKS;
      supablock.i_list = 1;

      init_data_structure();

      //init the root i-node here
      inode *root = &inodes_table.table[0];
      memcpy(&root->path,"/",1);
      root->st_mode = S_IFDIR;
      root->size = 0;
      root->links = 2;
      root->created = time(NULL);
      root->blocks = 0;
      root->uid = getuid();
      root->gid = getgid();
      root->type = 0;  // directory

      set_inode_bit(0,1); // set the bit map for root

      block_write(0, &supablock) > 0;

      block_write(1, &inodes_bm) > 0;

      block_write(2, &block_bm) > 0;

      int i = 0, j = 0;
      uint8_t *buffer = malloc(BLOCK_SIZE);
      for(; i < 64; i++)
      {
        int block_left = BLOCK_SIZE;
        while(block_left >= sizeof(struct inode_))
		{
          memcpy((buffer+(BLOCK_SIZE - block_left)), &inodes_table.table[j], sizeof(struct inode_));
          block_left -= sizeof(struct inode_);
          j++;
        }
        //write the block
		block_write(i+3, buffer);
      }
      free(buffer);
    }else{
      //read the superblock bitmaps and inodes from the disk file

      //check  the super block reading
      struct superblock *sb = (struct superblock*) buf;
      
      uint8_t *buffer = malloc(BLOCK_SIZE*sizeof(uint8_t));

      if(block_read(1, buffer) > 0)
	  {
        memcpy(&inodes_bm,buffer, sizeof(struct i_bitmap));
        memset(buffer,0,BLOCK_SIZE);
      }

      if(block_read(2, buffer)>0){
        memcpy(&block_bm, buffer, sizeof(struct block_bitmap));
        memset(buffer, 0, BLOCK_SIZE);
       }

      //load all the inodes
      int i;
      int k = 0;
      for(i = 0; i< 64; i++)
      {
        int offset = 0;
        if(block_read(i+3, buffer) > 0)
        {
			while(offset < BLOCK_SIZE && (BLOCK_SIZE - offset)>=sizeof(struct inode_)){
            memcpy(&inodes_table.table[k], buffer+offset, sizeof(struct inode_));
            k++;
            offset+=sizeof(struct inode_);
          }
        }
      }

      free(buffer);

    }
    free(buf);

    log_conn(conn);
    log_fuse_context(fuse_get_context());

    return SFS_DATA;
}

/**
 * Clean up filesystem
 *
 * Called on filesystem exit.
 *
 * Introduced in version 2.3
 */
void sfs_destroy(void *userdata)
{
    disk_close();
    
    log_msg("\nsfs_destroy(userdata=0x%08x)\n", userdata);
}

/** Get file attributes.
 *
 * Similar to stat().  The 'st_dev' and 'st_blksize' fields are
 * ignored.  The 'st_ino' field is ignored except if the 'use_ino'
 * mount option is given.
 */
int sfs_getattr(const char *path, struct stat *statbuf)
{
    int retstat = 0;
    char fpath[PATH_MAX];
 
   log_msg("\nsfs_getattr(path=\"%s\", statbuf=0x%08x)\n",
    path, statbuf);
   
    //search for inode
    int inode = get_inode_from_path(path);
    memset(statbuf,0,sizeof(struct stat));
    if(inode!=-1)
    {
      struct inode_ *tmp = &inodes_table.table[inode];
      statbuf->st_uid = tmp->uid;
      statbuf->st_gid = tmp->gid;
      statbuf->st_mode = tmp->st_mode;
      statbuf->st_nlink = tmp->links;
      statbuf->st_ctime = tmp->created;
      statbuf->st_size = tmp->size;
      statbuf->st_blocks = tmp->blocks;
    }
	else
	{
      retstat = -ENOENT;
    }

    return retstat;
}

/**
 * Create and open a file
 *
 * If the file does not exist, first create it with the specified
 * mode, and then open it.
 *
 * If this method is not implemented or under Linux kernel
 * versions earlier than 2.6.15, the mknod() and open() methods
 * will be called instead.
 *
 * Introduced in version 2.5
 */
int sfs_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
    int retstat = 0;
    log_msg("\nsfs_create(path=\"%s\", mode=0%03o, fi=0x%08x)\n",
      path, mode, fi);

    int i = get_inode_from_path(path);
    if(i == -1)
    {
      struct inode_ *tmp = malloc(sizeof(struct inode_));
      tmp->id = find_empty_inode_bit();
      tmp->size = 0;
      tmp->uid = getuid();
      tmp->gid = getgid();
      tmp->type = TYPE_FILE;
      tmp->links = 1;
      tmp->blocks = 0;
      tmp->st_mode = mode;
      memcpy(tmp->path, path,64);
      if(S_ISDIR(mode)) {
        tmp->type = TYPE_DIRECTORY;
      }

      memcpy(&inodes_table.table[tmp->id], tmp, sizeof(struct inode_));
      struct inode_ *in = &inodes_table.table[tmp->id];
      set_inode_bit(tmp->id, 1);
      free(tmp);
       
    
      block_write(1, &inodes_bm);
      uint8_t *buf = malloc(BLOCK_SIZE*sizeof(uint8_t));
      if(block_read(3+((in->id)/2), buf)>-1)  //e.g. inode 0 and 1 should be in block 0+2
      {
        int offset = (in->id%(BLOCK_SIZE/sizeof(struct inode_)))*sizeof(struct inode_);
        memcpy(buf+offset, in, sizeof(struct inode_));
        if(block_write(3+((in->id)/2), buf)>0)
		{
          fi->fh = 0;
        }
		else 
		{
			retstat = -EFAULT;
		}
      }
      free(buf);

    }else
	{
      retstat = -EEXIST;
    }
    
    return retstat;
}

/** Remove a file */
int sfs_unlink(const char *path)
{
    int retstat = 0;
    log_msg("\n\nsfs_unlink(path=\"%s\")\n", path);
	
    int i = get_inode_from_path(path);
    if(i!=-1)
    {
      struct inode_ *ptr = &inodes_table.table[i];
      set_inode_bit(ptr->id, 0);
      memset(ptr->path, 0, 64);
	  
      int j;
      for(j = 0; j<15;j++)
      {
        set_block_bit(ptr->data_blocks[j],0);
        ptr->data_blocks[j] = -1;
      }
	  
      write_inode_to_disk(ptr->id);
      block_write(1, &inodes_bm);
      block_write(2, &block_bm);
    }

    return retstat;
}

/** File open operation
 *
 * No creation, or truncation flags (O_CREAT, O_EXCL, O_TRUNC)
 * will be passed to open().  Open should check if the operation
 * is permitted for the given flags.  Optionally open may also
 * return an arbitrary filehandle in the fuse_file_info structure,
 * which will be passed to all file operations.
 *
 * Changed in version 2.2
 */
int sfs_open(const char *path, struct fuse_file_info *fi)
{
    int retstat = 0;
    log_msg("\n\nsfs_open(path\"%s\", fi=0x%08x)\n",
      path, fi);
	  
    int i = get_inode_from_path(path);
    if(i != -1)
    {
      retstat = get_empty_fd();
      if(retstat != -1)
	  {
		  take_fd(retstat,i);
	  }
    }
	else
	{
      retstat = -1;
    }
    
    return retstat;
}

/** Release an open file
 *
 * Release is called when there are no more references to an open
 * file: all file descriptors are closed and all memory mappings
 * are unmapped.
 *
 * For every open() call there will be exactly one release() call
 * with the same flags and file descriptor.  It is possible to
 * have a file opened more than once, in which case only the last
 * release will mean, that no more reads/writes will happen on the
 * file.  The return value of release is ignored.
 *
 * Changed in version 2.2
 */
int sfs_release(const char *path, struct fuse_file_info *fi)
{
    int retstat = 0;
    log_msg("\nsfs_release(path=\"%s\", fi=0x%08x)\n",
    path, fi);

    int i = get_inode_from_path(path);
    if(i != -1)
    {
      int file_d = find_fd(i);
      if(file_d != -1)
	  {
        fd_t *f = &fd.table[file_d];
        int temp = f->inode_id;
        f->inode_id = -1;
      }
    }
    

    return retstat;
}

/** Read data from an open file
 *
 * Read should return exactly the number of bytes requested except
 * on EOF or error, otherwise the rest of the data will be
 * substituted with zeroes.  An exception to this is when the
 * 'direct_io' mount option is specified, in which case the return
 * value of the read system call will reflect the return value of
 * this operation.
 *
 * Changed in version 2.2
 */
int sfs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    int retstat = 0;
    log_msg("\nsfs_read(path=\"%s\", buf=0x%08x, size=%d, offset=%lld, fi=0x%08x)\n",
      path, buf, size, offset, fi);
	  
    int i = get_inode_from_path(path);
    if(i != -1)
    {
      int file_d = find_fd(i);
      if(file_d!=-1)
      {
        struct inode_ *ptr = &inodes_table.table[i];
		
        if(ptr->size<=BLOCK_SIZE)
		{
          char *temp = malloc(size);
          if(block_read(ptr->data_blocks[0]+3+TOTAL_INODE_NUMBER, temp)>-1)
          {
            memcpy(buf,temp, size);
            retstat = size;
          }
          free(temp);
        }
		else
		{
          char *temp = malloc(size);
          int offset = 0;
          struct inode_ *ptr = &inodes_table.table[i];
          int j = 0;
          while(offset<ptr->size && j < 15 && (ptr->size-offset) >= BLOCK_SIZE)
          {
            if(block_read(ptr->data_blocks[j]+3+TOTAL_INODE_NUMBER, temp+offset) > -1)
            {
                j++;
                offset += BLOCK_SIZE; 
            }
          }
          if(offset < ptr->size)
          {
            char *buffer = malloc(BLOCK_SIZE);
            if(block_read(ptr->data_blocks[j]+3+TOTAL_INODE_NUMBER, buffer)>-1)
			{
              memcpy(temp+offset, buffer, ptr->size-offset);
            }
            free(buffer);
          }
          memcpy(buf,temp,size);
          free(temp);
        }
      }
    }
   
    return size;
}

/** Write data to an open file
 *
 * Write should return exactly the number of bytes requested
 * except on error.  An exception to this is when the 'direct_io'
 * mount option is specified (see read operation).
 *
 * Changed in version 2.2
 */
int sfs_write(const char *path, const char *buf, size_t size, off_t offset,
       struct fuse_file_info *fi)
{
    int retstat = 0;
    log_msg("\nsfs_write(path=\"%s\", buf=0x%08x, size=%d, offset=%lld, fi=0x%08x)\n",
      path, buf, size, offset, fi);
    
    int i = get_inode_from_path(path);
    if(i != -1)
    {
      int file_d = find_fd(i);
      if(file_d!=-1)
      {
        struct inode_ *ptr = &inodes_table.table[i];
       
	   if(ptr->size == 0)
	   {
          ptr->data_blocks[0] = find_empty_data_bit();
          set_block_bit(ptr->data_blocks[0],1);
          if(size <= BLOCK_SIZE)
          {
            if(block_write(3+TOTAL_INODE_NUMBER+ptr->data_blocks[0], buf) >= size)
			{
              ptr->size = size;
              ptr->modified = time(NULL);
              block_write(2, &block_bm);
              write_inode_to_disk(ptr->id);
              retstat = size;
              write_inode_to_disk(ptr->id);
              
            }
          }
		  else
		  {              
            int needed = size/BLOCK_SIZE;
            if((size-needed*BLOCK_SIZE)>0)
			{
				needed++;
			}  
            block_write(ptr->data_blocks[0]+TOTAL_INODE_NUMBER+3, buf);
              
            retstat+=BLOCK_SIZE;
            int offset = BLOCK_SIZE;
            int block;
            for(block = 1; block < needed; block++)
            {
              ptr->data_blocks[block] = find_empty_data_bit();
              set_block_bit(ptr->data_blocks[block],1);
              
              if(block_write(ptr->data_blocks[block]+TOTAL_INODE_NUMBER+3, buf+offset) > 0)
			  {
                offset+=BLOCK_SIZE;
              }
              
              if(block == needed-1) //the last block
              {
                if(offset == needed*BLOCK_SIZE)
                {
                  ptr->modified = time(NULL);
                  ptr->size = size;
                  retstat = size;
                  write_inode_to_disk(ptr->id);
                }
                else{
                  retstat = -1;
                }
              }
            }
          }
        }
		else
		{
          int blocks = ptr->size/BLOCK_SIZE;
          int offset = 0;
          if(ptr->size > blocks*BLOCK_SIZE)
		  {
            int off = ptr->size - blocks*BLOCK_SIZE;
            
            if(BLOCK_SIZE-off>=size)
			{ 
              char *buffer = malloc(BLOCK_SIZE);
              if(block_read(3+TOTAL_INODE_NUMBER+ptr->data_blocks[blocks], buffer)>-1)
              {
                memcpy(buffer+off, buf, BLOCK_SIZE-off);
                if(block_write(3+TOTAL_INODE_NUMBER+ptr->data_blocks[blocks],buffer)>-1)
                {
                  offset +=(BLOCK_SIZE-off);
                  ptr->size+=size;
                  write_inode_to_disk(ptr->id);
                }
              }
              free(buffer);
              return size;
            } 
			else
			{
              char *buffer = malloc(BLOCK_SIZE);
              if(block_read(3+TOTAL_INODE_NUMBER+ptr->data_blocks[blocks], buffer)>-1)
              {
                memcpy(buffer+off, buf, BLOCK_SIZE-off);
                if(block_write(3+TOTAL_INODE_NUMBER+ptr->data_blocks[blocks],buffer)>-1)
                {
                  offset +=(BLOCK_SIZE-off);
                  write_inode_to_disk(ptr->id);
                }
              }
              blocks++;
              int needed = (size - offset)/BLOCK_SIZE;
              if((size - offset) > needed*BLOCK_SIZE)
			  {
                needed++;
              }
              int b;
              for(b = blocks; b<blocks+needed; b++)
              {
                ptr->data_blocks[b] = find_empty_data_bit();
                set_block_bit(ptr->data_blocks[b],1);
                if(block_write(ptr->data_blocks[b]+TOTAL_INODE_NUMBER+3, buf+offset) > 0)
				{
                  offset += BLOCK_SIZE;
                }
                
                if(b == blocks+needed-1) //the last block
                {
                  if(offset >= needed*BLOCK_SIZE)
                  {
                    ptr->modified = time(NULL);
                    ptr->size += size;
                    retstat = size;
                    write_inode_to_disk(ptr->id);
                  }
                  else
				  {
                    retstat = -1;
                  }
				}
			  }
              free(buffer);
              return size;
            }
          }
        }

      }
	  else
	  {
        retstat = -1;
      }
    }
	else
	{
      retstat = -1;
    }
 
    return retstat;
}


/** Create a directory */
int sfs_mkdir(const char *path, mode_t mode)
{
    int retstat = 0;
    log_msg("\nsfs_mkdir(path=\"%s\", mode=0%3o)\n",
      path, mode);
	  
    int i = get_inode_from_path(path);
    if(i == -1)
	{  
      struct inode_ *tmp = malloc(sizeof(struct inode_));
      tmp->id = find_empty_inode_bit();
      tmp->size = 0;
      tmp->uid = getuid();
      tmp->gid = getgid();
      tmp->type = TYPE_FILE;
      tmp->links = 1;
      tmp->blocks = 0;
      tmp->st_mode = mode | S_IFDIR;
      memcpy(tmp->path, path,64);
      tmp->created = time(NULL);
      memcpy(&inodes_table.table[tmp->id], tmp, sizeof(struct inode_));
      set_inode_bit(tmp->id, 1);            
      write_inode_to_disk(tmp->id);
      free(tmp);
      block_write(1, &inodes_bm);

    }else{
      retstat = -EEXIST;
    }

    
    return retstat;
}


/** Remove a directory */
int sfs_rmdir(const char *path)
{
    int retstat = 0;
    log_msg("sfs_rmdir(path=\"%s\")\n",
      path);

    int i = get_inode_from_path(path);
    if(i!=-1){
      int j;
      for(j=0;j<TOTAL_INODE_NUMBER;j++)
      {
        if(((inodes_bm.bitmap[j / 8] >> (j % 8)) & 1) != 0 && j != i)
        {
          if(check_parent_dir(path, j)!=-1)
          {
            log_msg("DIR not empty!\n");
            return -ENOTEMPTY;
          }
        }
      }
      struct inode_ *ptr = &inodes_table.table[i];
      set_inode_bit(ptr->id, 0);
      memset(ptr->path, 0, 64);
      for(j = 0; j<15;j++)
      {
        set_block_bit(ptr->data_blocks[j],0);
        ptr->data_blocks[j] = -1;
      }
      log_msg("Inode %d delete complete!\n\n",ptr->id);
      write_inode_to_disk(ptr->id);
      block_write(1, &inodes_bm);
      block_write(2, &block_bm);
    }else{
      return -ENOENT;
    }
    
    return retstat;
}


/** Open directory
 *
 * This method should check if the open operation is permitted for
 * this  directory
 *
 * Introduced in version 2.3
 */
int sfs_opendir(const char *path, struct fuse_file_info *fi)
{
    int retstat = 0;
    log_msg("\nsfs_opendir(path=\"%s\", fi=0x%08x)\n",
    path, fi);
	
    int i = get_inode_from_path(path);
    if(i == -1)
    {
        return -ENOENT;
    }
	
    return retstat;
}

/** Read directory
 *
 * This supersedes the old getdir() interface.  New applications
 * should use this.
 *
 * The filesystem may choose between two modes of operation:
 *
 * 1) The readdir implementation ignores the offset parameter, and
 * passes zero to the filler function's offset.  The filler
 * function will not return '1' (unless an error happens), so the
 * whole directory is read in a single readdir operation.  This
 * works just like the old getdir() method.
 *
 * 2) The readdir implementation keeps track of the offsets of the
 * directory entries.  It uses the offset parameter and always
 * passes non-zero offset to the filler function.  When the buffer
 * is full (or an error happens) the filler function will return
 * '1'.
 *
 * Introduced in version 2.3
 */
int sfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset,
         struct fuse_file_info *fi)
{
    int retstat = 0;
    
    filler(buf,".", NULL, 0);  
    filler(buf, "..", NULL, 0);
    int i = 0;
    for(;i<TOTAL_INODE_NUMBER;i++)
    {
      if(((inodes_bm.bitmap[i / 8] >> (i % 8)) & 1) != 0)
      {
        if(check_parent_dir(path, i)!=-1 && strcmp(inodes_table.table[i].path, path)!=0)
        {
          char* name =get_file_name(i);
          struct stat *statbuf = malloc(sizeof(struct stat));
          inode *tmp = &inodes_table.table[i];
          statbuf->st_uid = tmp->uid;
          statbuf->st_gid = tmp->gid;
          statbuf->st_mode = tmp->st_mode;
          statbuf->st_nlink = tmp->links;
          statbuf->st_ctime = tmp->created;
          statbuf->st_size = tmp->size;
          statbuf->st_blocks = tmp->blocks;
          filler(buf,name,statbuf,0);
          free(name);
          free(statbuf);
        }
      }
    }

    return retstat;
}

/** Release directory
 *
 * Introduced in version 2.3
 */
int sfs_releasedir(const char *path, struct fuse_file_info *fi)
{
    int retstat = 0;

    
    return retstat;
}

struct fuse_operations sfs_oper = {
  .init = sfs_init,
  .destroy = sfs_destroy,

  .getattr = sfs_getattr,
  .create = sfs_create,
  .unlink = sfs_unlink,
  .open = sfs_open,
  .release = sfs_release,
  .read = sfs_read,
  .write = sfs_write,

  .rmdir = sfs_rmdir,
  .mkdir = sfs_mkdir,

  .opendir = sfs_opendir,
  .readdir = sfs_readdir,
  .releasedir = sfs_releasedir
};

void sfs_usage()
{
    fprintf(stderr, "usage:  sfs [FUSE and mount options] diskFile mountPoint\n");
    abort();
}

int main(int argc, char *argv[])
{
    int fuse_stat;
    struct sfs_state *sfs_data;
    
    // sanity checking on the command line
    if ((argc < 3) || (argv[argc-2][0] == '-') || (argv[argc-1][0] == '-'))
  sfs_usage();

    sfs_data = malloc(sizeof(struct sfs_state));
    if (sfs_data == NULL) {
  perror("main calloc");
  abort();
    }

    // Pull the diskfile and save it in internal data
    sfs_data->diskfile = argv[argc-2];
    argv[argc-2] = argv[argc-1];
    argv[argc-1] = NULL;
    argc--;
    
    sfs_data->logfile = log_open();
    
    // turn over control to fuse
    fprintf(stderr, "about to call fuse_main, %s \n", sfs_data->diskfile);
    fuse_stat = fuse_main(argc, argv, &sfs_oper, sfs_data);
    fprintf(stderr, "fuse_main returned %d\n", fuse_stat);
    
    return fuse_stat;
}
