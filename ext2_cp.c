#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "ext2_welp.h"

char *usage = "USAGE: %s disk src dest\n";

int ext2_cp(unsigned char *disk, char *src, char *dest) {
	// Check source
	struct stat sb;
	if (stat(src, &sb)) {
		fprintf(stderr, "Source file does not exist\n");
		return ENOENT;
	}
	
	if (S_ISDIR(sb.st_mode)) {
		fprintf(stderr, "Source is a directory\n");
		return EISDIR;
	}

	// Check destination, if file or directory exist
	struct ext2_dir_entry_2 *entry = navigate(disk, dest);
	if (!entry) {
		char *dir = get_dir(dest);
		entry = navigate(disk, dir);

		if (!entry) {
			fprintf(stderr, "%s is not a directory\n", dir);
			free(dir);
			return ENOENT;
		}
		free(dir);
	}

	char *name = get_filename(src);

	// Check if file exist, if so overwrite
	if (EXT2_IS_DIRECTORY(entry)) {
		struct ext2_dir_entry_2 *_entry = find_file(disk, get_inode(disk, entry->inode), name);
		if (_entry) entry = _entry;
	}

	// Open file
	FILE *file = fopen(src, "r");
	assert(file);

	struct ext2_inode *inode = get_inode(disk, entry->inode);
	if (EXT2_IS_FILE(entry)) {
		// Clean entry
		free_blocks(disk, entry->inode);
	} else {
		// Get new inode
		entry = add_thing(disk, entry, name, EXT2_FT_REG_FILE);
		inode = get_inode(disk, entry->inode);

		// Setup inode
		inode->i_mode = EXT2_S_IFREG;
		inode->i_links_count = 1;
		inode->i_ctime = time(0);
	}

	// Set information
	inode->i_size = sb.st_size;
	inode->i_atime = time(0);
	inode->i_mtime = time(0);

	char *buff = (char *)malloc(EXT2_BLOCK_SIZE);
	int block_index, i = 0;

	while (fread(buff, 1, EXT2_BLOCK_SIZE, file)) {
		block_index = get_free_block(disk);
		set_block_bitmap(disk, block_index, 1);

		memcpy(EXT2_BLOCK(disk, block_index), buff, EXT2_BLOCK_SIZE);

		// Add to direct
		if (i < EXT2_DIRECT_BLOCKS) {
			inode->i_block[i] = block_index;
		
		// Add to indirect
		} else {

			// Init indirect
			if (i == EXT2_DIRECT_BLOCKS) {
				int indirect = get_free_block(disk);
				set_block_bitmap(disk, indirect, 1);
				inode->i_block[EXT2_DIRECT_BLOCKS] = indirect;
			}

			int *indirect_block = (int *)EXT2_BLOCK(disk, inode->i_block[EXT2_DIRECT_BLOCKS]);
			indirect_block[i - EXT2_DIRECT_BLOCKS] = block_index;
		}

		i++;
	}

	EXT2_SET_BLOCKS(inode, MIN(i, EXT2_DIRECT_BLOCKS + 1));
	free(buff);
	free(name);
	return 0;
}

int main(int argc, char *argv[]) {
	// Check args
	if (argc != 4) {
		fprintf(stderr, usage, argv[0]);
		return 1;
	}

	// Read disk
	unsigned char *disk = read_image(argv[1]);
	return ext2_cp(disk, argv[2], argv[3]);
}