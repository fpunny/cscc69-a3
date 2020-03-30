
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
	unsigned int   inode;     /* Inode number = use get_inode function (4 Byte Alingment, 0, 12, 24, 40, 52, 68)
	unsigned short rec_len;   /* Directory entry length = Record Length (Meaning space until next inode)
		If prev dir starts at 0 and next dir starts at 12, then rec len for prev dir is 12.
	unsigned char  name_len;  /* Name length = argc length
	unsigned char  file_type; = #define    EXT2_FT_DIR      2    /* Directory File
	char           name[];    /* File name, up to EXT2_NAME_LEN 255 = argc (No \0 for the names!)
};
 * Type field for file mode
#define    EXT2_S_IFDIR  0x4000    /* directory
Might need to set this to parent Directory ACL
unsigned int   i_dir_acl;     /* Directory ACL

struct ext2_inode {
	unsigned short i_mode;        /* File mode  = 
	unsigned short i_uid;         /* Low 16 bits of Owner Uid
	unsigned int   i_size;        /* Size in bytes = 
	unsigned int   i_atime;       /* Access time
	unsigned int   i_ctime;       /* Creation time 
	unsigned int   i_mtime;       /* Modification time 
	unsigned int   i_dtime;       /* Deletion Time 
	unsigned short i_gid;         /* Low 16 bits of Group Id 
	unsigned short i_links_count; /* Links count = 
	unsigned int   i_blocks;      /* Blocks count IN DISK SECTORS = 
	unsigned int   i_flags;       /* File flags 
	unsigned int   osd1;          /* OS dependent 1
	unsigned int   i_block[15];   /* Pointers to blocks =
	unsigned int   i_generation;  /* File version (for NFS) 
	unsigned int   i_file_acl;    /* File ACL 
	unsigned int   i_dir_acl;     /* Directory ACL 
	unsigned int   i_faddr;       /* Fragment address 
	unsigned int   extra[3];
};

Notes:
ext2 attempts to allocate each new directory in the group containing its parent directory
However, if the group is full, then the new file or new directory is placed in some other non-full group.

The data blocks needed to store directories and files can be found by looking in the data allocation bitmap. 
Any needed space in the inode table can be found by looking in the inode allocation bitmap.
*/
int main(int argc, char *argv[]) {
	/* Error Checking (2 Edge Cases) */
	/* Check if path does not exist by calling Navigate, if null return ENOENT */
	/* Check if specified directory already exists by calling find_file, if true return EEXIST */
	
	/* At this point, the path exists and the specified directly doesn't exist yet so we will create it */
	
	exit(0);
}
