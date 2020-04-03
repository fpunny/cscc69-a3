#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include "ext2.h"
#include "ext2_welp.h"
char *usage = "USAGE: %s disk [-r] path\n";

int remove_file_or_dir(unsigned char *disk, char *path, int r_flag) {
	// Get entry
	struct ext2_dir_entry_2 *entry = navigate(disk, path);
	if (!entry) {
		fprintf(stderr, "No such file or directory\n");
		return ENOENT;
	}

	// Get dir
	path = get_dir(path);
	struct ext2_dir_entry_2 *dir = navigate(disk, path);
	free(path);
	
	// Remove thing
	remove_entry(disk, dir, entry);
	return 0;
}

int main(int argc, char *argv[]) {
	unsigned char *disk;
	unsigned r_flag = 0;
	char *path;

	if (argc == 3) {
		disk = read_image(argv[1]);
		path = argv[2];
	} else if (argc == 4 && !strcmp("-r", argv[2])) {
		disk = read_image(argv[1]);
		path = argv[3];
		r_flag = 1;
	} else {
		fprintf(stderr, usage, argv[0]);
		return 1;
	}

	if (strcmp(path, "/") == 0) {
		fprintf(stderr, "Cannot delete root directory\n");
		return EPERM;
	}

	return remove_file_or_dir(disk, path, r_flag);
}


