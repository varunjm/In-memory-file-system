all:	c r
v: valgrind r

valgrind:
	valgrind ./ramdisk -s -f mountdir 10
c:
	gcc -g -Wall ramdisk.c `pkg-config fuse --cflags --libs` -o ramdisk
r:
	./ramdisk -s -f mountdir 10
u:
	fusermount -u mountdir


