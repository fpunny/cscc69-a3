#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include "ext2_welp.h"

char *usage = "USAGE: %s disk [-a] path\n";

int ext2_ls(unsigned char *disk, char *path, int flag_a) {

	// Navigate to the directory of path
	struct ext2_dir_entry_2 *entry = navigate(disk, path);
	if (!entry) {
		printf("No such file or directory\n");
		return -ENOENT;
	}

	if (EXT2_IS_DIRECTORY(entry)) {
		struct ext2_inode *inode = get_inode(disk, entry->inode);
		int *blocks = inode_to_blocks(disk, inode);
		int limit = EXT2_NUM_BLOCKS(disk, inode);
		struct ext2_dir_entry_2 *block;
		int i;

		for (i = 0; i < limit; i++) {
			block = (struct ext2_dir_entry_2 *)EXT2_BLOCK(disk, blocks[i]);
			printf("%s\n", block->name);
		}

	} else {
		printf("%s\n", entry->name);
	}

	return 0;
}

int main(int argc, char *argv[]) {
	unsigned char *disk;
	int flag_a = 0;
	char *path;

	// If run without -a argument
	if (argc == 3 && strcmp(argv[2], "-a") != 0) {
		disk = read_image(argv[1]);
		path = argv[2];

	// If run with -a argument
	} else if (argc == 4 && strcmp(argv[2], "-a") == 0) {
		disk = read_image(argv[1]);
		path = argv[3];
		flag_a = 1;
	
	// Anything else
	} else {
		printf(usage, argv[0]);
		return 1;
	}

	if (disk == MAP_FAILED) {
        perror("mmap");
        return 1;
    }

	return ext2_ls(disk, path, flag_a);
}