#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include "ext2.h"
#include "ext2_welp.h"

char *usage = "USAGE: %s disk path\n";

int check_file_type(unsigned char *disk, char *path) {

	int status = 0;
	char* saved_path = malloc(strlen(path));
        strncpy(saved_path, path, strlen(path));
		
	// Get the block from the given path
	struct ext2_dir_entry_2 *entry = navigate(disk, saved_path);
	if (entry == NULL) {
		fprintf(stderr, "ext2_rm: Invalid file or directory '%s'\n", path);
		return ENOENT;
	}
	if (EXT2_IS_DIRECTORY(entry)) {
		fprintf(stderr, "ext2_rm: cannot remove '%s': Is a directory\n", path);
		return EISDIR;
	}
	
	free(saved_path);
	return status;
	
}

int main(int argc, char *argv[]) {
	
	unsigned char *disk;
	char *path;

	if (argc == 3)	{
		disk = read_image(argv[1]);
		path = argv[2];
	} 
	else
	{
		printf(usage, argv[0]);
		exit(1);
	}

	if (disk == MAP_FAILED) {
		perror("mmap");
		exit(1);
	}
	
	if (check_file_type(disk, path) == 0) {
		return remove_file(disk, path);
	}
	return 1;
}
