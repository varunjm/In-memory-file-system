## In Memory File system

This is an implementation of an in-memory File system using FUSE

It supports the following File System operations:
* open
* close
* read
* write
* mknod
* mkdir
* unlink
* rmdir
* opendir
* readdir

### Usage

```shell
$ make
$ ./ramdisk <directory-to-mount> <size in MB>
```

Any of the previously listed operations done within this directory is done thorugh ramdisk.
