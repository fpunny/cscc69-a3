#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include "ext2.h"
#include "ext2_welp.h"
char *usage = "USAGE: %s disk [-r] path\n";

int remove_file_or_dir(unsigned char *disk, char* path, int r_flag) {
	struct ext2_dir_entry_2 *block;
	int i, j;
	//save path so it doesnt mess with other iterations
	int path_len = strlen(path);
	char* saved_path = malloc(path_len + 1);
	strncpy(saved_path, path, path_len + 1);

	if (r_flag == 0) {
		remove_file(disk, saved_path);
	} else {
		
		struct ext2_dir_entry_2 *entry = navigate(disk, path);
		if (!EXT2_IS_DIRECTORY(entry)) {
			remove_file(disk, saved_path);
		} else {
			struct ext2_inode *inode = get_inode(disk, entry->inode);
			int *blocks = inode_to_blocks(disk, inode);
		        int limit = EXT2_NUM_BLOCKS(disk, inode);
			
			strcat(saved_path, "/");
			char* end = saved_path + path_len;
			
			// Loop through blocks
		        for (i = 0; i < limit; i++) {
			    block = (struct ext2_dir_entry_2 *)EXT2_BLOCK(disk, blocks[i]);

			    // Loop through entries in block
			    for (j = 0; j < EXT2_BLOCK_SIZE; (j += block->rec_len, block = EXT2_NEXT_FILE(block))) {
				if (strcmp(block->name, ".") == 0
				|| strcmp(block->name, "..") == 0
				|| block->name_len <= 0) {
					continue;
				}
				// append file name to path
				strncat(saved_path, block->name, block->name_len);
				// Remove all files and dirs in sub directory
				
				remove_file_or_dir(disk, saved_path, r_flag);
				*end = '\0';
			    }
		        }
			free(blocks);
			// .. gets removed when deleting a directory, so decrement parent link count
			struct ext2_dir_entry_2 *parent = get_parent(disk, saved_path);
			struct ext2_inode *parent_inode = get_inode(disk, parent->inode);
			parent_inode->i_links_count --;

			// Remove this directory
			remove_file(disk, saved_path);
		}
	}
	
	return 0;
}

int main(int argc, char *argv[]) {
	
	unsigned char *disk;
	char *path;
	char r_flag = 0;
	disk = read_image(argv[1]);
	path = argv[2];
	
	if (argc < 3 || argc > 4) {
		fprintf(stderr, usage, argv[0]);
		exit(1);
	} else if (argc == 4 && strcmp("-r", argv[2]) == 0) {
		r_flag = 1;
		path = argv[3];
	}

	if (disk == MAP_FAILED) {
		perror("mmap");
		exit(1);
	}

	if (strcmp(path, "/") == 0) {
		fprintf(stderr, "ext2_rm: Cannot delete root directory\n");
		return EPERM;
	}

	return remove_file_or_dir(disk, path, r_flag);
}


