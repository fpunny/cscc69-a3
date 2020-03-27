#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include "ext2_welp.h"

char *usage = "USAGE: %s disk src dest\n";

int ext2_cp(unsigned char *disk, char *src, char *dest) {
	
}

int main(int argc, char *argv[]) {
	unsigned char *disk;
	char *src, *dest;

	// If run without -a argument
	if (argc != 3) {
		printf(usage, argv[0]);
		return 1;
	}

	disk = read_image(argv[1]);
	dest = argv[3];
	src = argv[2];

	if (disk == MAP_FAILED) {
        perror("mmap");
        return 1;
    }

	return ext2_cp(disk, src, dest);
}