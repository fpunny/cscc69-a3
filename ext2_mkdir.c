
#include <stdio.h>
#include "ext2_welp.h"

char *usage = "USAGE: %s disk path\n";

int main(int argc, char *argv[]) {
	if (argc == 3) {
		unsigned char *disk = read_image(argv[1]);

		// Check if root
		if (argv[2][strlen(argv[2]) - 1] == '/') {
			fprintf(stderr, "Please provide a directory name\n");
			return ENONET;
		}

		// Check if exist
		struct ext2_dir_entry_2 *entry = navigate(disk, argv[2]);
		if (entry) {
			fprintf(stderr, "%s already exists\n", argv[2]);
			return EEXIST;
		}
		
		// Get directory
		char *dir_path = get_dir(argv[2]);
		entry = navigate(disk, dir_path);

		if (!entry) {
			fprintf(stderr, "No such directory\n");
			free(dir_path);
			return ENOENT;
		}
		
		if (!EXT2_IS_DIRECTORY(entry)) {
			fprintf(stderr, "%s is not a directory", dir_path);
			free(dir_path);
			return ENOTDIR;
		}
		free(dir_path);

		// Create new Dir with given name
		char *dir_name = get_filename(argv[2]);
		struct ext2_dir_entry_2 *new_dir_entry = add_thing(disk, entry, dir_name, EXT2_FT_DIR);
		free(dir_name);

		// Setup directory
		struct ext2_inode *new_dir_inode = get_inode(disk, new_dir_entry->inode);
		EXT2_SET_BLOCKS(new_dir_inode, 0);
		new_dir_inode->i_mode = EXT2_S_IFDIR;
		new_dir_inode->i_links_count = 2;

		// Add the . Shortcut
		struct ext2_dir_entry_2 *curr_dir_link = add_thing(disk, new_dir_entry, ".", EXT2_FT_DIR);
		set_inode_bitmap(disk, curr_dir_link->inode, 0);
		curr_dir_link->inode = new_dir_entry->inode;

		// Add the .. Shortcut
		struct ext2_dir_entry_2 *parent_dir_link = add_thing(disk, new_dir_entry, "..", EXT2_FT_DIR);
		set_inode_bitmap(disk, parent_dir_link->inode, 0);
		parent_dir_link->inode = entry->inode;
		get_inode(disk, entry->inode)->i_links_count++;

		return 0;
	}

	printf(usage, argv[0]);
	return 1;
}
