/*
FUSE: Filesystem in Userspace
Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

This program can be distributed under the terms of the GNU GPL.
See the file COPYING.

gcc -Wall ramdisk.c `pkg-config fuse --cflags --libs` -o ramdisk
*/

#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>

#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>

static const char *ramdisk_path = "/ramdisk";

enum FILETYPES{REGULAR,DIRECTORY,FIFO,BLK,CHR,HLINK,SLINK};

typedef struct node {
	char *name;						
	int fileType;
	mode_t mode;
	int nLink;
	int size;
	int fileCount;
	int fileListSize;
	struct node **fileList;
	struct node *parent;
	char *data;
}FS;

FS rootDir, *pwd;

void initialize(FS *temp,const char *n, int fT, mode_t mode, int s, FS *p)
{
	temp->name = (char *)malloc(strlen(n)+1);
	strcpy(temp->name,n);
	temp->fileType = fT;
	if(S_ISFIFO(mode))
		temp->mode = S_IFIFO | mode;
	else if(S_ISDIR(mode))
		temp->mode = S_IFDIR | mode;
	temp->size = 0;
	temp->parent = p;
	temp->fileCount = 0;
	temp->fileListSize = 1;
	pwd = &rootDir;
}

static void ramdisk_init()
{
	rootDir.name = (char *)malloc(2);
	strcpy(rootDir.name,"/");
	rootDir.parent = NULL;
	rootDir.fileCount = 0;
	rootDir.fileListSize = 1;
	rootDir.fileList = (FS **)malloc(sizeof(FS *));
}

int clip(const char *path)
{
	int i = 0, j = 0;
	while(path[i] != '\0')
	{
		if(path[i] == '/')
			j = i;
		i++;
	}
	return j;
}

int typeFlag(int mode)
{
	if(S_ISFIFO(mode))
		return S_IFIFO;
	else if(S_ISBLK(mode))
		return S_IFBLK;
	else if(S_ISCHR(mode))
		return S_IFCHR;
	else if(S_ISLNK(mode))
		return S_IFLNK;
	else
		return S_IFREG;
}

FS * pathExists(const char *path, FS * temp)
{
	int i;
	// printf("Entering PathExists : %s, %s\n",path,temp->name);

	if(!strcmp(path,temp->name))
	{
		// printf("Exiting pathExists : %s, %s\n",path,temp->name );
		return temp;
	}
	for(i=0;i<temp->fileCount;i++)
	{
		// printf("%s\n",temp->fileList[i]->name);
		if(!strcmp(path,temp->fileList[i]->name))
		{
			// printf("Exiting PathExists : %s, %s\n",path,temp->name);
			return temp->fileList[i];
		}
		if( temp->fileList[i]->fileType == DIRECTORY )
		{
			FS *t = pathExists(path,temp->fileList[i]);
			if(t)
			{
				// printf("Exiting PathExists : %s, %s\n",path,temp->name);
				return t;
			}	
		}
	}
	// printf("Exiting PathExists : %s, %s\n",path,temp->name);
	return NULL;
}

FS * pathExists1(const char *path, FS * temp)
{
	int i;
	printf("Entering PathExists : %s, %s\n",path,temp->name);

	if(!strcmp(path,temp->name))
	{
		printf("Exiting pathExists : %s, %s\n",path,temp->name );
		return temp;
	}
	for(i=0;i<temp->fileCount;i++)
	{
		printf("%s\n",temp->fileList[i]->name);
		if(!strcmp(path,temp->fileList[i]->name))
		{
			printf("Exiting PathExists : %s, %s\n",path,temp->name);
			return temp->fileList[i];
		}
		if( temp->fileList[i]->fileType == DIRECTORY )
		{
			FS *t = pathExists1(path,temp->fileList[i]);
			if(t)
			{
				printf("Exiting PathExists : %s, %s\n",path,temp->name);
				return t;
			}	
		}
	}
	// printf("Exiting PathExists : %s, %s\n",path,temp->name);
	return NULL;
}

static int ramdisk_getattr(const char *path, struct stat *stbuf)
{
	FS *entry;
	printf("Entering getAttr : %s\n",path);
	memset(stbuf, 0, sizeof(struct stat));
	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
	} 
	else if ( (entry = pathExists(path,&rootDir)) != NULL ) 
	{
		if( entry->fileType == DIRECTORY )
		{
			printf("DIRECTORY\n");
			stbuf->st_mode = S_IFDIR | 0755;
			stbuf->st_nlink = 2;
		}
		else 
		{
			printf("NON DIRECTORY : %o\n",typeFlag(entry->fileType));
			stbuf->st_mode = typeFlag(entry->fileType) | 0644;
			stbuf->st_nlink = 1;	
		}
		stbuf->st_size = entry->size;
	}
	else
	{
		printf("Not found : %s\n",path);
		return -ENOENT;
	}
	printf("Found : %s\n",path);
	return 0;
}

static int ramdisk_mkdir(const char *path, mode_t mode)
{
	FS *temp = (FS *)malloc(sizeof(FS)), *entry;
	printf("Entering mkdir() : %s\n",path );
	
	char pathClipped[1024];

	int i = 0, j;
	
	i = clip(path);

	for(j=0; j<i; j++)
		pathClipped[j] = path[j];
	if(j==0)
		entry = &rootDir;
	else
	{
		pathClipped[j] = '\0';
		entry = pathExists(pathClipped,&rootDir);
	}
	
	
	if(entry)
	{
		entry->fileCount++;
		initialize(temp,path,DIRECTORY,mode,0,entry);
		if( entry->fileListSize <= entry->fileCount)
		{
			entry->fileList = (FS **)realloc(entry->fileList, (entry->fileListSize)*2*sizeof(FS));
			entry->fileListSize *= 2;
		}
		entry->fileList[(entry->fileCount)-1] = temp;
		printf("created dir : %s, under %s\n",path,entry->name);
		printf("Exiting mkdir()\n");
		return 0;
	}
	else
		printf("Exiting mkdir(), did not create dir : %s\n",path );
		return -ENOENT;
}

static int ramdisk_mknod(const char *path, mode_t mode)
{
	FS *temp = (FS *)malloc(sizeof(FS)), *entry;
	printf("Entering mknod() : %s, %d\n",path,S_ISFIFO(mode) );
	
	char pathClipped[1024];

	int i = 0, j;
	
	i = clip(path);

	for(j=0; j<i; j++)
		pathClipped[j] = path[j];
	if(j==0)
		entry = &rootDir;
	else
	{
		pathClipped[j] = '\0';
		entry = pathExists(pathClipped,&rootDir);
	}

	if(entry)
	{
		(entry->fileCount)++;

		if(S_ISFIFO(mode))
			initialize(temp,path,FIFO,mode,0,entry);
		else if(S_ISBLK(mode))
			initialize(temp,path,BLK,mode,0,entry);
		else if(S_ISCHR(mode))
			initialize(temp,path,CHR,mode,0,entry);
		else if(S_ISLNK(mode))
			initialize(temp,path,SLINK,mode,0,entry);
		else
			initialize(temp,path,REGULAR,mode,0,entry);

		if( entry->fileListSize <= entry->fileCount)
		{
			entry->fileList = (FS **)realloc(entry->fileList, (entry->fileListSize)*2*sizeof(FS));
			entry->fileListSize *= 2;
		}
		entry->fileList[(entry->fileCount)-1] = temp;
		printf("created file : %s, under %s\n",path,entry->name);
		printf("Exiting mknod()\n");
		return 0;
	}
	else
		printf("Exiting mknod(), did not create file : %s\n",path );
		return -ENOENT;
}

static int ramdisk_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi)
{
	(void) offset;
	(void) fi;
	int i, j;

	printf("Entering readDir : %s\n",path);
	FS *temp = pathExists(path, &rootDir);
	
	if (!temp)
		return -ENOENT;
	else
	{
		filler(buf, ".", NULL, 0);
		filler(buf, "..", NULL, 0);
		for( i=0 ; i<temp->fileCount ; i++ )
		{
			j = clip(temp->fileList[i]->name);
			printf("%s : %d\n",path,j );
			printf("Adding to filler : %s\n",(temp->fileList[i]->name)+j+1);
			filler(buf, (temp->fileList[i]->name)+j+1, NULL, 0);
		}
	}
	printf("Exiting readDir : %s\n",path);
	return 0;
}
/*
static int ramdisk_open(const char *path, struct fuse_file_info *fi)
{
	if (strcmp(path, ramdisk_path) != 0)
		return -ENOENT;

	if ((fi->flags & 3) != O_RDONLY)
		return -EACCES;
	return 0;
}*/



static int ramdisk_read(const char *path, char *buf, size_t size, off_t offset,
		    struct fuse_file_info *fi)
{
	int fd;
	int res;

	(void) fi;
	printf("Entering read : %s, %s\n",path,buf);

	FS * temp = pathExists(path, &rootDir);

	if(!temp)
		res = -errno;
	else
	{
		strncpy(buf,(temp->data)+offset,size);
		res = size;
	}
	return res;
}

static int ramdisk_write(const char *path, const char *buf, size_t size,
		     off_t offset, struct fuse_file_info *fi)
{
	int res=0, i=0;
	(void) fi;
	printf("Entering write : %s, %s\n",path,buf);

	FS * temp = pathExists(path, &rootDir);

	if(temp->size == 0)
	{
		temp->data = (char *)malloc(size);
		temp->size = size;
	}
	else if( temp->size < (offset+size) )
	{
		temp->data = (char *)realloc(temp->data,offset+size);
		temp->size = offset+size+1;
	}

	if(temp->data == NULL)
		return -ENOSPC;

	printf("Writing data : %s\n", buf);
	printf("Size : %d, %d, %c\n",temp->size,size,temp->data[2] );
	while(i<size)
	{
		temp->data[offset+i] = buf[i];
		i++;
	}
	if(temp->size == size)
		temp->data[size] = '\0';

	printf("Written in file : %s\n",temp->data );	
	return size;
}


static int ramdisk_statfs(const char *path, struct statvfs *stbuf)
{
	int res;

	res = statvfs(path, stbuf);
	if (res == -1)
		return -errno;

	return 0;
}

int ramdisk_opendir(const char *path, struct fuse_file_info *fi)
{
    int retstat = 0;
    char fpath[1024];
    
    strcpy(fpath,"/home/varun/Desktop/local/Operating System Principles/P4");
    strcat(fpath,path);

    // since opendir returns a pointer, takes some custom handling of
    // return status.
    fi->fh = (intptr_t) opendir(fpath);
    
    return retstat;
}

static struct fuse_operations ramdisk_oper = {
	.getattr	= ramdisk_getattr,
	
	// .open		= ramdisk_open,
	// .flush		= ramdisk_flush,

	.read		= ramdisk_read,
	.write		= ramdisk_write,
	
	.mknod		= ramdisk_mknod,
	.mkdir		= ramdisk_mkdir,

	// .unlink 	= ramdisk_unlink,
  	// .rmdir 		= ramdisk_rmdir,

	.opendir    = ramdisk_opendir,
	.readdir	= ramdisk_readdir,


	.statfs 	= ramdisk_statfs,
};

int main(int argc, char *argv[])
{
	/*if(argc < 2)
	{
		fprintf(stderr,"Usage: ramdisk <mount directory>\n");
		return 1;
	}*/
	printf("start\n");
	ramdisk_init();
	return fuse_main(argc, argv, &ramdisk_oper, NULL);
}
