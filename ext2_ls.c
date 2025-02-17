#include <stdio.h>
#include "ext2_welp.h"

char *usage = "USAGE: %s disk [-a] path\n";

int print(struct ext2_dir_entry_2 *block, void *flag_a) {
	char *name = get_name(block);
	if (*(int *)flag_a || (strcmp(name, ".") != 0 && strcmp(name, "..") != 0)) {
		printf("%s\n", name);
	}
	free(name);
	return 1;
}

int ext2_ls(unsigned char *disk, char *path, int flag_a) {

	// Navigate to the directory of path
	struct ext2_dir_entry_2 *entry = navigate(disk, path);
	if (!entry) {
		fprintf(stderr, "No such file or directory\n");
		return ENOENT;
	}

	// If directory, print it, else the file itself
	if (EXT2_IS_DIRECTORY(entry)) {
		struct ext2_inode *inode = get_inode(disk, entry->inode);
		iterate_inode(disk, inode, print, &flag_a);
	} else {
		char *name = get_name(entry);
		printf("%s\n", name);
		free(name);
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
		fprintf(stderr, usage, argv[0]);
		return 1;
	}

	return ext2_ls(disk, path, flag_a);
}