
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

Details:
 Links
Unix filesystems implement the concept of link. Several names can be associated with a inode. 
The inode contains a field containing the number associated with the file. 
Adding a link simply consists in creating a directory entry, 
where the inode number points to the inode, 
and in incrementing the links count in the inode. When a link is deleted, 
i.e.when one uses the rm command to remove a filename, 
the kernel decrements the links count and deallocates the inode if this count becomes zero.

This type of link is called a hard link and can only be used within a single filesystem: 
it is impossible to create cross-filesystem hard links. 
Moreover, hard links can only point on files: a directory hard link cannot be created to prevent the apparition of a 
cycle in the directory tree.

Another kind of links exists in most Unix filesystems. 
Symbolic links are simply files which contain a filename. 
When the kernel encounters a symbolic link during a pathname to inode conversion, 
it replaces the name of the link by its contents, i.e. the name of the target file, and restarts the pathname interpretation. 
Since a symbolic link does not point to an inode, it is possible to create cross-filesystems symbolic links. 
Symbolic links can point to any type of file, even on nonexistent files. 
Symbolic links are very useful because they don't have the limitations associated to hard links. However, they use some disk space,
allocated for their inode and their data blocks, and cause an overhead in the pathname to inode conversion because the kernel has to
restart the name interpretation when it encounters a symbolic link.

Keep in mind:
when creating a new hard link for a file, the counter of hard links in
the disk inode is incremented first, and the new name is added into the proper directory next.
*/
int main(int argc, char *argv[]) {
	/* Error Checking */
	/* Check if correct number of arguments passed */
	if (argc == 3 || argc == 4) {
		/* Check if source file does not exist, if true, return ENOENT */
		/* Check if link name already exists, if true, return EEXIST */
		/* Check if location refers to a director using helper EXT2_IS_DIRECTORY, if true, return EISDIR */
	
		/* Check if Soft / Symbolic Link */
		if (argc == 4 && strcmp(argv[3], "s") == 0) {
		
		} else {
			/* Hard Links Instruction Flow (Order Matters) */
			/* Increment the counter of hard links in the disk inode */
			/* Add the new name to the proper directory */
		}
	} else {
		return 1;
	}
	exit(0);
}
