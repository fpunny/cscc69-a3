#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>
#include "ext2_welp.h"

char *usage = "USAGE: %s disk src dest\n";

char *getName(char *path) {
	char *token = strtok(path, "/");
	char *_token;

	do {
		_token = strtok(NULL, "/");
		if (_token) token = _token;
	} while (_token);
	return token;
}

int ext2_cp(unsigned char *disk, char *src, char *dest) {
	// Get file info
	struct stat sb;
	char *name;
	if (stat(src, &sb) || !S_ISREG(sb.st_mode)) {
		printf("Invalid source\n");
		return ENOENT;
	}

	// Navigate to the dest
	name = getName(dest);
	struct ext2_dir_entry_2 *entry = navigate(disk, dest);
	if (!entry) {
		int end = strlen(dest) - strlen(name);
		char dir[EXT2_NAME_LEN];
		strncpy(dir, dest, end);
		dir[end] = '\0';

		entry = navigate(disk, dir);

		if (!entry) {
			printf("Invalid destination\n");
			return ENOENT;
		}
	}

	// If file already exist, overwrite it
	name = getName(src);
	struct ext2_dir_entry_2 *check = find_file(disk, get_inode(disk, entry->inode), name);
	if (check) {
		entry = check;
	}

	// Open file
	FILE *file = fopen(src, "r");
	assert(file);

	struct ext2_inode *inode = get_inode(disk, entry->inode);
	if (EXT2_IS_FILE(entry)) {
		// Clean entry
		remove_file(disk, entry->inode, 1);
	} else {
		// Get new inode
		entry = add_thing(disk, entry, name, EXT2_FT_REG_FILE);
		inode = get_inode(disk, entry->inode);

		// Setup inode
		memset(inode, '\0', sizeof(struct ext2_inode));
		inode->i_mode = EXT2_S_IFREG;
		inode->i_links_count = 1;
		inode->i_ctime = time(0);
	}

	// Set information
	inode->i_size = sb.st_size;
	inode->i_atime = time(0);
	inode->i_mtime = time(0);

	char *buff = malloc(EXT2_BLOCK_SIZE * sizeof(char));
	int block_index, i = 0;

	while (fread(buff, sizeof(char), EXT2_BLOCK_SIZE, file)) {
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
				inode->i_block[EXT2_DIRECT_BLOCKS] = indirect;
			}

			int *indirect_block = (int *)EXT2_BLOCK(disk, inode->i_block[EXT2_DIRECT_BLOCKS]);
			indirect_block[i - EXT2_DIRECT_BLOCKS] = block_index;
		}

		i++;
	}

	EXT2_SET_BLOCKS(inode, MIN(i, EXT2_DIRECT_BLOCKS));
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