#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include "ext2_welp.h"

char *usage = "USAGE: %s disk src dest\n";

char *getName(char *path) {
	char *token = strtok(path, "/");
	while (token) {
		char *_token = strtok(NULL, "/");
		if (_token) token = _token;
	}
	return token;
}

int ext2_cp(unsigned char *disk, char *src, char *dest) {
	// Get file info
	struct stat sb;
	if (stat(src, &sb) || !S_ISREG(sb.st_mode)) {
		perror(src);
		return ENOENT;
	}

	// Navigate to the dest
	struct ext2_dir_entry_2 *entry = navigate(disk, dest);
	if (!entry) {
		perror(dest);
		return ENOENT;
	}

	// Get free inode
	int inode_index = get_free_inode(disk);
	if (inode_index == -1) {
		perror("inode");
		return ENOENT;
	}

	// Open file
	FILE *file = fopen(src, "r");
	assert(file);

	return 0;
}

int main(int argc, char *argv[]) {
	// Check args
	if (argc != 4) {
		printf(usage, argv[0]);
		return 1;
	}

	// Read disk
	unsigned char *disk = read_image(argv[1]);
	if (disk == MAP_FAILED) {
        perror("mmap");
        return 1;
    }

	return ext2_cp(disk, argv[2], argv[3]);
}