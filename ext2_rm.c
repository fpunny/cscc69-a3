#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include "ext2.h"
#include "ext2_welp.h"

char *usage = "USAGE: %s disk path\n";

int main(int argc, char *argv[]) {
	struct ext2_dir_entry_2 *entry, *dir;
	unsigned char *disk;
	char *path;

	if (argc == 3)	{
		disk = read_image(argv[1]);
		path = argv[2];
		
		// Check path
		entry = navigate(disk, path);
		if (!entry) {
			fprintf(stderr, "'%s': Invalid file or directory\n", path);
			return ENOENT;
		}
		if (EXT2_IS_DIRECTORY(entry)) {
			fprintf(stderr, "'%s': Is a directory\n", path);
			return EISDIR;
		}

		// Get directory containing file
		path = get_dir(path);
		dir = navigate(disk, path);
		free(path);

		remove_entry(disk, dir, entry);
		return 0;
	}

	fprintf(stderr, usage, argv[0]);
	return 1;
}
