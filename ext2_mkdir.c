
#include <stdio.h>
#include "ext2_welp.h"
/*
ext2_mkdir: This program takes two command line arguments. 
The first is the name of an ext2 formatted virtual disk. 
The second is an absolute path on your ext2 formatted disk.
The program should work like mkdir, creating the final directory on the specified path on the disk. 
If any component on the path to the location where the final directory is to be created 
	does not exist or 
if the specified directory already exists, 
	then your program should return the appropriate error (ENOENT or EEXIST).
Again, please read the specifications to make sure you're implementing everything correctly 
(e.g., directory entries should be aligned to 4B, entry names are not null-terminated, etc.).

Helpful:
We need this for init
struct ext2_dir_entry_2 {
	unsigned int   inode;     Inode number = use get_inode function (4 Byte Alingment, 0, 12, 24, 40, 52, 68)
	unsigned short rec_len;   Directory entry length = Record Length (Meaning space until next inode)
		If prev dir starts at 0 and next dir starts at 12, then rec len for prev dir is 12.
	unsigned char  name_len;  Name length = argc length
	unsigned char  file_type; = #define    EXT2_FT_DIR      2    Directory File
	char           name[];    File name, up to EXT2_NAME_LEN 255 = argc (No \0 for the names!)
};
 * Type field for file mode
#define    EXT2_S_IFDIR  0x4000    directory
Might need to set this to parent Directory ACL
unsigned int   i_dir_acl;     Directory ACL

struct ext2_inode {
	unsigned short i_mode;        File mode  = 
	unsigned short i_uid;         Low 16 bits of Owner Uid
	unsigned int   i_size;        Size in bytes = 
	unsigned int   i_atime;       Access time
	unsigned int   i_ctime;       Creation time 
	unsigned int   i_mtime;       Modification time 
	unsigned int   i_dtime;       Deletion Time 
	unsigned short i_gid;         Low 16 bits of Group Id 
	unsigned short i_links_count; Links count = 
	unsigned int   i_blocks;      Blocks count IN DISK SECTORS = 
	unsigned int   i_flags;       File flags 
	unsigned int   osd1;          OS dependent 1
	unsigned int   i_block[15];   Pointers to blocks =
	unsigned int   i_generation;  File version (for NFS) 
	unsigned int   i_file_acl;    File ACL 
	unsigned int   i_dir_acl;     Directory ACL 
	unsigned int   i_faddr;       Fragment address 
	unsigned int   extra[3];
};

Notes:
ext2 attempts to allocate each new directory in the group containing its parent directory
However, if the group is full, then the new file or new directory is placed in some other non-full group.

The data blocks needed to store directories and files can be found by looking in the data allocation bitmap. 
Any needed space in the inode table can be found by looking in the inode allocation bitmap.
*/

char *usage = "USAGE: %s disk path\n";

char *get_dir(char *path) {
	char *_path = (char *)malloc((strlen(path) + 1) * sizeof(char));
	strncpy(_path, path, strlen(path) + 1);

	char *token = strtok(_path, "/");
	char *_token = NULL;
	int i = 1;

	do {
		_token = strtok(NULL, "/");
		if (_token) {
			i += strlen(token) + 1;
			token = _token;
		}
	} while (_token);

	strncpy(_path, path, i);
	_path[i - 1] = '\0';

	return _path;
}

char *getName(char *path) {
	char *token = strtok(path, "/");
	char *_token;

	do {
		_token = strtok(NULL, "/");
		if (_token) token = _token;
	} while (_token);
	return token;
}

int main(int argc, char *argv[]) {
	if (argc == 3) {
		unsigned char *disk = read_image(argv[1]);

		if (argv[2][strlen(argv[2]) - 1] == '/') {
			printf("Please provide a directory name\n");
			return ENONET;
		}

		struct ext2_dir_entry_2 *entry = navigate(disk, argv[2]);
		if (entry) {
			printf("%s already exists\n", argv[2]);
			return EEXIST;
		}
		
		char *dir_path = get_dir(argv[2]);
		entry = navigate(disk, dir_path);
		free(dir_path);
		
		// Error Checking (2 Edge Cases)
		// Check if path does not exist by calling Navigate, if null return ENOENT
		if (entry == NULL || !EXT2_IS_DIRECTORY(entry)) {
			printf("No such directory\n");
			return ENOENT;
		}

		// Create new Dir with given name
		char *dir_name = getName(argv[2]);
		struct ext2_dir_entry_2 *new_dir_entry = add_thing(disk, entry, dir_name, EXT2_FT_DIR);

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
