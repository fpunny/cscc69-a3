
#include <stdlib.h>
#include "ext2.h"

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
	unsigned int   inode;     /* Inode number = use get_inode function
	unsigned short rec_len;   /* Directory entry length = EXT2 BLock
	unsigned char  name_len;  /* Name length = argc length
	unsigned char  file_type; = #define    EXT2_FT_DIR      2    /* Directory File
	char           name[];    /* File name, up to EXT2_NAME_LEN = argc
};

*/
int main(int argc, char *argv[]) {
	/* Check if path does not exist by calling Navigate, if null return ENOENT */
	/* Check if specified directory already exists by calling find_file, if true return EEXIST */
	/* At this point, the path exists and the specified directly doesn't exist yet so we will create it */
	exit(0);
}
