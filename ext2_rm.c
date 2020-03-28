#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>
#include "ext2.h"
#include "ext2_welp.h"

char *usage = "USAGE: %s disk path\n";

int ext2_rm(unsigned char *disk, char *path) {
	int i;
	unsigned int num_blocks;
	int *blocks;
	int block_pos;
	int bit_pos;
	int byte;
	int inode_num;
	char* saved_path;
	// navigate uses strtok which splits string, so save path before using it
	saved_path = malloc(sizeof(char)* strlen(path));
	strncpy(saved_path, path, strlen(path));
	// Get the block from the given path
	struct ext2_dir_entry_2 *entry = navigate(disk, path);
	// Get parent directory to iterate through blocks and find
	// prev and curr block
	// Find last / in saved_path
	char *first_slash = strrchr(saved_path,'/');
	// Set the character at this point to '\0', thus terminating the string
	*first_slash = '\0';
	// saved_path variable is now set to the parent directory, so get parent block
	printf("%s\n", saved_path);
	struct ext2_dir_entry_2 *parent = navigate(disk, saved_path);
	free(saved_path);
	
	// Get the inode from the block
	struct ext2_inode *inode = get_inode(disk, entry->inode);
	// Get a list of blocks that are being used by this entry
	blocks = inode_to_blocks(disk, inode);
	// Get the number of blocks being used by this entry
	num_blocks = EXT2_NUM_BLOCKS(disk, inode);
	// Get the super block
	struct ext2_super_block *super = EXT2_SUPER_BLOCK(disk);
	// Get the block group
	struct ext2_group_desc *blgrp = EXT2_GROUP_DESC(disk);
	// Get the block bitmap
        unsigned char * block_bm = ( unsigned char * )(disk + EXT2_BLOCK_SIZE  * blgrp->bg_block_bitmap);
	// Set blocks to free
	for (i = 0; i < num_blocks; i ++) {
		// Get the byte where the block is in the bitmap.
		// For example, with block 25:
		// 25 / 8 = 3, so in the bitmap, the 3rd index is where the bit mapping to this block is stored
		block_pos = blocks[i] / 8;
		// Get the bit that maps this to this block in the bitmap.
		// For example, with block 25:
		// 25 % 8 = 1. So we need to change the 1st bit from the left
		// -1 because bits are 0 indexed
		bit_pos = (blocks[i] % 8) - 1;
		printf("bit pos %d\t", bit_pos);
		// GEt the number representing the bits at this point in the bitmap
		byte = block_bm[block_pos];
		// Set bit to off
		byte = byte & ~(1 << bit_pos);
		// set new value in bitmap
		block_bm[block_pos] = byte;
		// increase free blocks count
		blgrp->bg_free_blocks_count ++;
		super->s_free_blocks_count ++;
	}
	// Set inode to free, same logic as blocks
	inode_num = entry->inode;
	printf("inode_num: %d ", inode_num);
	unsigned char *inode_bm = ( unsigned char * )(disk + EXT2_BLOCK_SIZE  * blgrp->bg_inode_bitmap);
	block_pos = inode_num / 8;
	bit_pos = (inode_num % 8) -1;
	byte = inode_bm[block_pos];
	byte = byte & ~(1 << bit_pos);
	inode_bm[block_pos] = byte;
	blgrp->bg_free_inodes_count ++;
	super->s_free_inodes_count ++;

	// Set file type to undef
	entry->file_type = EXT2_FT_UNKNOWN;
	// Set size to 0
	inode->i_size = 0;
	// Set links to 0
	inode->i_links_count = 0;
	// set delete time
	time_t delete_time = time(NULL);
	if (delete_time != (time_t)(-1)) {
		inode-> i_dtime = (intmax_t)time;
	} else {
		fprintf(stderr, "Error: Failed to set delete time in inode %d\n", inode_num);	
	}
	
	// set entry's inode to 0
	// entry->inode=0;
	return 0;
	// add to rec_len of previous dir entry so that deleted file gets skipped
	
}

int main(int argc, char *argv[]) {
	
	unsigned char *disk;
	char *path;

	if (argc == 3)	{
		disk = read_image(argv[1]);
		path = argv[2];
	} 
	else
	{
		printf(usage, argv[0]);
		exit(1);
	}

	if (disk == MAP_FAILED) {
		perror("mmap");
		exit(1);
	}

	return ext2_rm(disk, path);
}
