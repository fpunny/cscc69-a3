
#include <stdlib.h>
#include "ext2.h"
/*
ext2_ln: This program takes three command line arguments. 
The first is the name of an ext2 formatted virtual disk. 
The other two are absolute paths on your ext2 formatted disk. 
Additionally, this command may take a "-s" flag, after the disk image argument. 
	When this flag is used, your program must create a symlink instead (other arguments remain the same). 
	If in doubt about correct operation of links, use the ext2 specs and ask on the discussion board.
	
The program should work like ln, creating a link from the first specified file to the second specified path. 

If the source file does not exist (ENOENT), 
if the link name already exists (EEXIST), or 
if either location refers to a directory (EISDIR), 
then your program should return the appropriate error. 
Note that this version of ln only works with files. 

unsigned int   i_block[15];   /* Pointers to blocks
unsigned short i_links_count; /* Links count
#define    EXT2_S_IFLNK  0xA000    /* symbolic link
#define    EXT2_FT_SYMLINK  7    /* Symbolic Link
*/
int main(int argc, char *argv[]) {
	/* Check if source file does not exist, if true, return ENOENT */
	/* Check if link name already exists, if true, return EEXIST */
	/* Check if location refers to a director using helper EXT2_IS_DIRECTORY, if true, return EISDIR */
	exit(0);
}
