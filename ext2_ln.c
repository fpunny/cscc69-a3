
#include <stdlib.h>
#include "ext2.h"
#include "ext2_welp.h"

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

Example Case:
echo "hello world" > ../superLongFileName.txt
To make not have to call the superlongfilename everytime we do
ln -s ../superLongFileName.txt repText.txt
now when we do ls, we will see repText.txt as a newly created link file.

Soft Link = ShortCuts (very small, Different Inode Number)
Hard Link = Different Name of the Same File, Same File Size, SAME iNODE NUMBER (Like a copy of a file)
*/
char *usage = "USAGE: %s disk [-s] target_path link_name\n";

int ext2_ln(unsigned char *disk, char *source_path, char *target_path, int is_soft_link) {
	// The file we want to LINK TO
	struct ext2_dir_entry_2 *source_entry = navigate(disk, source_path);
	// The file we are going to CREATE which is a LINK
	struct ext2_dir_entry_2 *target_entry = navigate(disk, target_path);
	
	/* Check if source file does not exist, if true, return ENOENT */
	if (source_entry == NULL) {
		printf("No such file\n");
		return ENOENT;
	}
	/* Check if link name already exists, if true, return EEXIST */
	char* target_name = get_name(target_entry);
	struct ext2_dir_entry_2 *check_unique_name = find_file(disk, get_inode(disk, target_entry->inode), target_name)
	if (check_unique_name) {
		return EEXIST;
	}
	
	/* Check if location refers to a director using helper EXT2_IS_DIRECTORY, if true, return EISDIR */
	if (EXT2_IS_DIRECTORY(target_entry)) {
		return EISDIR;
	}
	
	if (is_soft_link) {
		
		free(target_name);
		return ;
	} else {
		
		free(target_name);
		return ;
	}
}


int main(int argc, char *argv[]) {
	unsigned char *disk;
	int is_soft_link = 1; // 1 = Not Soft Link
	char *source_path;
	char *target_path;
	/* Error Checking */
	/* Check if correct number of arguments passed */
	if (argc == 4 || argc == 5){
		
		disk = read_image(argv[1]);
		
		
		/* Check if Hard Link OR Soft / Symbolic Link ELSE Fail*/
		if (argc == 3 && strcmp(argv[2], "s") != 0) {
			source_path = argv[2];
			target_path = argv[3];			
			/* Hard Links Instruction Flow (Order Matters) */
			/* Increment the counter of hard links in the disk inode */
			/* Add the new name to the proper directory */
			ext2_ln(disk, source_path, target_path, is_soft_link);
		} else if (argc == 4 && strcmp(argv[2], "s") == 0) {
			source_path = argv[3];
			target_path = argv[4];
			is_soft_link = 0; // Soft Link = True
			ext2_ln(disk, source_path, target_path, is_soft_link);
		} else {
			return 1;
		}
	} else {
		printf(usage, argv[0]);
		return 1;
	}
	return ext2_ln(disk, source_path, target_path, is_soft_link);
}
