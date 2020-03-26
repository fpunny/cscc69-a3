#include <sys/mman.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include "ext2_welp.h"
#include "ext2.h"

char *usage = "USAGE: %s disk [-a] path\n";
unsigned char *disk;
int flag_a = 0;
char *path;

void ext2_ls() {
	// Read super and group descriptor blocks
	struct ext2_group_desc *blgrp = EXT2_GROUP_DESC(disk);
	struct ext2_super_block *sb = EXT2_SUPER_BLOCK(disk);
	struct ext2_dir_entry_2 entry = navigate(disk, path);

	exit(0);
}

int main(int argc, char *argv[]) {

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
		exit(1);
	}

	printf("%s %d\n", path, flag_a);
	ext2_ls();
}