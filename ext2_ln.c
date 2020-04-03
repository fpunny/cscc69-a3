
#include <stdio.h>
#include <time.h>
#include "ext2_welp.h"

char *usage = "USAGE: %s disk [-s] target_path link_name\n";

void create_soft(unsigned char *disk, struct ext2_inode *entry, char *path) {
    int len = strlen(path);
    char *_path = path;

	// Remove prefix /
    if (path[0] == '/') {
        _path++;
        len--;
    }

	// Remove suffix /
    if (path[len - 1] == '/') {
        len--;
    }

	// Add ze link
    memcpy(EXT2_BLOCK(disk, entry->i_block[0]), _path, len);
    entry->i_size = len;
}

int ext2_ln(unsigned char *disk, char *src, char *target, unsigned is_soft) {
	// Get src and check it
	struct ext2_dir_entry_2 *source_entry = navigate(disk, src);
	if (source_entry) {
		fprintf(stderr, "File or directory already exist\n");
		return EEXIST;
	}

	// Check if directory exist
	char *dirpath = get_dir(src);
	source_entry = navigate(disk, dirpath);
	free(dirpath);

	if (!source_entry) {
		fprintf(stderr, "Invalid source path\n");
		return ENONET;
	}

	// Get target and check it
	struct ext2_dir_entry_2 *target_entry = navigate(disk, target);
	if (!target_entry) {
		fprintf(stderr, "No such target file\n");
		return ENOENT;
	}

	if (EXT2_IS_DIRECTORY(target_entry)) {
		fprintf(stderr, "Target is a directory\n");
		return EISDIR;
	}

	char *filename = get_filename(src);
	
	if (is_soft) {
		struct ext2_dir_entry_2 *new_soft_link = add_thing(disk, source_entry, filename, EXT2_FT_SYMLINK);
		struct ext2_inode *inode = get_inode(disk, new_soft_link->inode);

		// Get block
		int block_index = get_free_block(disk);
		set_block_bitmap(disk, block_index, 1);

		// Setup inode
		EXT2_SET_BLOCKS(inode, 1);
		inode->i_block[0] = block_index;
		inode->i_mode = EXT2_S_IFLNK;
		inode->i_links_count = 1;
		inode->i_ctime = time(0);
		inode->i_atime = time(0);
		inode->i_mtime = time(0);

		// Write into block
		create_soft(disk, inode, target);

	} else {
		struct ext2_dir_entry_2 *new_hard_link = add_thing(disk, source_entry, filename, EXT2_FT_REG_FILE);
		struct ext2_inode *inode = get_inode(disk, source_entry->inode);

		// Free inode from add
		set_inode_bitmap(disk, new_hard_link->inode, 0);
		new_hard_link->inode = target_entry->inode;
		inode->i_links_count++;
	}

	free(filename);
	return 0;
}


int main(int argc, char *argv[]) {
	unsigned char *disk;

	// If hard link
	if (argc == 4) {
		disk = read_image(argv[1]);
		return ext2_ln(disk, argv[2], argv[3], 0);
	
	// If soft link
	} else if (argc == 5 && strcmp(argv[2], "-s") == 0) {
		disk = read_image(argv[1]);
		return ext2_ln(disk, argv[3], argv[4], 1);
	}

	// Bad input
	fprintf(stderr, usage, argv[0]);
	return 1;
}
