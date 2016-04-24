all:	c r
c:
	gcc -Wall ramdisk.c `pkg-config fuse --cflags --libs` -o ramdisk
r:
	./ramdisk -f mtdir
u:
	fusermount -u mtdir
