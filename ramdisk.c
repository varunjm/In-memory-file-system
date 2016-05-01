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

enum FILETYPES{REGULAR,DIRECTORY,FIFO,BLK,CHR,HLINK,SLINK};
long max_size;
long cur_size;

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

int initialize(FS *temp,const char *n, int fT, mode_t mode,FS *p)
{
	printf("Entering initialize : %s\n",n);
	// File name
	temp->name = (char *)malloc(strlen(n)+1);
	if(temp->name == NULL)
		return 1;
	strcpy(temp->name,n);
	
	//File type
	temp->fileType = fT;

	//File mode
	if(S_ISFIFO(mode))
		temp->mode = S_IFIFO | mode;
	else if(S_ISDIR(mode))
		temp->mode = S_IFDIR | mode;

	//File size
	temp->size = 0;

	// File data 
	temp->data = NULL;

	//File parent
	temp->parent = p;

	// Additional data if its a directory
	if(fT == DIRECTORY)
	{
		temp->fileCount = 0;
		temp->fileList = (FS **)malloc(sizeof(FS *));
		if(temp->fileList == NULL)
			return 1;
		temp->fileListSize = 1;
	}

	printf("Exiting initialize : %s\n",n);
	
	return 0;
}

static void ramdisk_init(long m_s)
{
	rootDir.name = (char *)malloc(2);
	strcpy(rootDir.name,"/");
	rootDir.parent = NULL;
	rootDir.fileType = DIRECTORY;
	rootDir.fileCount = 0;
	rootDir.fileListSize = 1;
	rootDir.fileList = (FS **)malloc(sizeof(FS *));
	pwd = &rootDir;
	cur_size = 0;
	max_size = m_s;
	if(rootDir.name == NULL || rootDir.fileList == NULL)
	{
		errno = ENOMEM;
		exit(1);
	}
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
		printf("Exiting pathExists 1: %s, %s\n",path,temp->name );
		return temp;
	}
	for(i=0;i<temp->fileCount;i++)
	{
		// printf("%s\n",temp->fileList[i]->name);
		if(!strcmp(path,temp->fileList[i]->name))
		{
			printf("Exiting PathExists 2: %s, %s\n",path,temp->name);
			return temp->fileList[i];
		}
		if( temp->fileList[i]->fileType == DIRECTORY )
		{
			FS *t = pathExists1(path,temp->fileList[i]);
			if(t)
			{
				printf("Exiting PathExists 3: %s, %s\n",path,temp->name);
				return t;
			}	
		}
	}
	printf("Exiting PathExists : NULL %s, %s\n",path,temp->name);
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
	printf("Entering mkdir() : %s\n",path );
	FS *temp = (FS *)malloc(sizeof(FS)), *entry;
	if(temp == NULL)	return -ENOMEM;

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
		if ( initialize(temp,path,DIRECTORY,mode,entry) )	return -ENOMEM;
		if( entry->fileListSize <= entry->fileCount)
		{
			entry->fileList = (FS **)realloc(entry->fileList, (entry->fileListSize)*2*sizeof(FS *));
			if(entry->fileList == NULL)	return -ENOMEM;
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

static int ramdisk_mknod(const char *path, mode_t mode, dev_t dev)
{

	printf("Entering mknod() : %s, %d\n",path,S_ISFIFO(mode) );
	FS *temp = (FS *)malloc(sizeof(FS)), *entry;
	if(temp == NULL)	return -ENOMEM;
	
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
			j = initialize(temp,path,FIFO,mode,entry);
		else if(S_ISBLK(mode))
			j = initialize(temp,path,BLK,mode,entry);
		else if(S_ISCHR(mode))
			j = initialize(temp,path,CHR,mode,entry);
		else if(S_ISLNK(mode))
			j = initialize(temp,path,SLINK,mode,entry);
		else
			j = initialize(temp,path,REGULAR,mode,entry);

		if(j)	return -ENOMEM;

		if( entry->fileListSize <= entry->fileCount)
		{
			printf("Before realloc : %s\n",entry->name);
			entry->fileList = (FS **)realloc(entry->fileList, (entry->fileListSize)*2*sizeof(FS *));
			if(!entry->fileList)	return -ENOMEM;
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

static int ramdisk_open(const char *path, struct fuse_file_info *fi)
{
	printf("Entering open : %s\n",path );
	FS * temp = pathExists(path,&rootDir);
	if(temp->fileType == DIRECTORY)
		return -EISDIR;
	// if( (temp->mode & S_IRUSR) == 0)
	// 	return -EACCES;
	printf("Exiting open : %s\n",path );
	return (temp==NULL) ? -ENOENT : 0;

}
static int ramdisk_read(const char *path, char *buf, size_t size, off_t offset,
		    struct fuse_file_info *fi)
{
	int res = 0;
	int i = 0, j ;
	(void) fi;
	printf("Entering read : %s, offset : %d\n",path,offset);

	FS * temp = pathExists(path, &rootDir);
	if(!temp) res = -ENOENT;
	else if(temp->data != NULL)
	{
		printf("%d, %d \n",offset+size,temp->size);
		if(offset >= temp->size )
			return 0;
		j = ((offset+size)>(temp->size))?temp->size:offset+size;
		for(;i<j;i++)
			buf[i] = temp->data[offset+i];
		buf[i] = '\0';
		res = i;
	}
	return res;
}

static int ramdisk_write(const char *path, const char *buf, size_t size,
		     off_t offset, struct fuse_file_info *fi)
{
	int i=0;
	(void) fi;
	printf("Entering write : %s, %s\n",path,buf);

	FS * temp = pathExists1(path, &rootDir);
	if(temp == NULL) return -ENOENT;



	if(temp->size == 0)
	{
		printf("1 : Here\n");
		if(max_size-cur_size < size)
			return -ENOMEM;
		printf("%d\n",size);
		temp->data = (char *)malloc(size);
		printf("After malloc%d\n",size);
		if(temp->data == NULL)	return -ENOMEM;
		temp->size = size;
		cur_size += size;
	}
	else if( temp->size < (offset+size) )
	{
		printf("2 : Here\n");
		if( max_size-cur_size < ((offset+size)-temp->size) )
			return -ENOMEM;
		printf("%d %d \n",max_size-cur_size,((offset+size)-temp->size) );
		printf("Reallocating memory : %s %d %d \n",temp->data,temp->size,offset+size);
		temp->data = (char *)realloc(temp->data,offset+size);
		printf("Executed realloc \n");
		if(temp->data == NULL) return -ENOMEM;
		printf("Reallocated memory : %s\n",temp->data);

		temp->size = offset+size;
		cur_size += offset + size - (temp->size);

	}

	if(temp->data == NULL)
		return -ENOSPC;

	printf("Writing data : %s\n", buf);
	printf("Size : %d, %d, %d\n",(int)temp->size,(int)size, strlen(buf));
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
/*
static int ramdisk_statfs(const char *path, struct statvfs *stbuf)
{
	int res;

	res = statvfs(path, stbuf);
	if (res == -1)
		return -errno;

	return 0;
}
*/
int ramdisk_opendir(const char *path, struct fuse_file_info *fi)
{
	printf("Entering opendir : %s\n",path );
   	FS * temp = pathExists(path,&rootDir);
   	if(temp == NULL)	return -ENOENT;
   	printf("%s %d\n",temp->name,temp->fileType);
	if(temp->fileType != DIRECTORY)
		return -ENOTDIR;

	printf("Exiting opendir : %s\n",path );
	return (temp==NULL) ? -ENOENT : 0;
}

static int ramdisk_unlink(const char *path)
{
    printf("Entering unlink : %s\n",path);
    FS * temp = pathExists(path,&rootDir);
    if(temp==NULL) return -ENOENT;
    FS * parent = temp->parent;
    int i=0;
    int size = temp->size;

    printf("before deleted file info : %s\n",temp->name);
	free(temp->data);
	free(temp->name);
    printf("deleted file info\n");

    if(parent->fileCount == 1)
    {
    	parent->fileCount = 0;
    	parent->fileListSize = 1;
    	free(parent->fileList);
    	parent->fileList = (FS **)malloc(sizeof(FS*));
    	if(parent->fileList == NULL)	return -ENOMEM;
    }
    else
    {
    	printf("Managing parent's fileList \n");
    	while(parent->fileList[i] != temp) i++;
    	printf("Freeing :  %s\n",parent->fileList[i]->name);
    	free(parent->fileList[i]);
    	(parent->fileCount)--;
    	
    	if( i != parent->fileCount )
    		parent->fileList[i] = parent->fileList[parent->fileCount];

    	if( (parent->fileListSize)/2 > parent->fileCount)
    	{
    		parent->fileListSize /= 2;
    		parent->fileList = (FS **)realloc(parent->fileList, sizeof(FS *)*parent->fileListSize);
    		if( parent->fileList == NULL ) return -ENOMEM;
    	}	
    	
    }
    cur_size -= size;
    printf("Exiting unlink : %s\n",path);

    return 0;
}

static int ramdisk_rmdir(const char *path)
{
    printf("Entering rmdir : %s\n",path);
    FS * temp = pathExists(path,&rootDir);
    if(temp == NULL) return -ENOENT;
    FS * parent = temp->parent;
    int i=0;

    if(temp->fileCount != 0)
    	return -ENOTEMPTY;


    free(temp->fileList);
    free(temp->name);

    if(parent->fileCount == 1)
    {
    	free(temp);
    	free(parent->fileList);
    	parent->fileCount = 0;
    	parent->fileList = (FS **)malloc(sizeof(FS*));
    	if(parent->fileList == NULL)	return -ENOMEM;
    	parent->fileListSize = 1;
    }
    else
    {
    	while(parent->fileList[i] != temp) i++;
    	free(temp);
    	(parent->fileCount)--;
    	if( i != parent->fileCount )
    		parent->fileList[i] = parent->fileList[parent->fileCount];
    	if(parent->fileListSize > 2*parent->fileCount)
    	{
    		parent->fileListSize /= 2;
    		parent->fileList = (FS **)realloc(parent->fileList,parent->fileListSize*sizeof(FS *));
    		if(parent->fileList == NULL) return -ENOMEM;
    	}
    }
    printf("Exiting rmdir : %s\n",path);

    return 0;
}

static int ramdisk_truncate(const char* path, off_t size)
{
	printf("Entering truncate : %s, %d\n",path,size );
	FS * temp = pathExists(path,&rootDir);
	if(!temp) return -ENOENT;

	char *data = temp->data;
	char *newData;

	if(size == 0)
	{
		if(data != NULL)
			free(data);
		cur_size -= temp->size;
		temp->size = 0;
		return 0;
	}
	else
	{
		newData = (char *)malloc(size);
		if(newData == NULL) return -ENOMEM;
		strncpy(newData,data,size);
		if(data != NULL)
			free(data);
		temp->data = newData;
		cur_size -= temp->size;
		temp->size = size;
		cur_size += temp->size;
	}
	
	printf("Exiting truncate %s, cur_size : %d\n",path,cur_size );
	return 0;
}

static int ramdisk_flush(const char* path, struct fuse_file_info* fi)
{
	return 0;
}

static struct fuse_operations ramdisk_oper = {
	.getattr	= ramdisk_getattr,
	
	.open		= ramdisk_open,
	.flush		= ramdisk_flush,
	.truncate 	= ramdisk_truncate,
	.read		= ramdisk_read,
	.write		= ramdisk_write,
	
	.mknod		= ramdisk_mknod,
	.mkdir		= ramdisk_mkdir,

	.unlink 	= ramdisk_unlink,
  	.rmdir 		= ramdisk_rmdir,

	.opendir    = ramdisk_opendir,
	.readdir	= ramdisk_readdir,


	// .statfs 	= ramdisk_statfs,
};

int main(int argc, char *argv[])
{
	/*if(argc < 3)
	{
		fprintf(stderr,"Usage: ramdisk <mount directory> <size in mega bytes>\n");
		return 1;
	}*/
	// ramdisk_init(atoi(argv[2])*1024*1024);
	int size = atoi(argv[4]);

	argc = 4;
	argv[4] = '\0';

	ramdisk_init( size*1024*1024 );
	return fuse_main(argc, argv, &ramdisk_oper, NULL);
}
