#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>
#include "ext2.h"
#include "ext2_welp.h"

char *usage = "USAGE: %s disk path\n";

int is_prev_directory(struct ext2_dir_entry_2 *entry, void * name) {
	char * file_name = (char *) name;
	struct ext2_dir_entry_2 *next_block = EXT2_NEXT_FILE(entry);
	if (strcmp(file_name, next_block->name) == 0) {
		return 0;
	}
	return 1;
}

int delete_entry(unsigned char* disk, struct ext2_inode *inode, struct ext2_dir_entry_2 * entry) {

	int i;
	unsigned int num_blocks;
	int *blocks;
	int block_pos;
	int bit_pos;
	int byte;
	int inode_num;
	int status = 0;
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
	// set delete time
	time_t delete_time = time(NULL);
	if (delete_time != (time_t)(-1)) {
		inode-> i_dtime = (intmax_t)time;
	} else {
		fprintf(stderr, "Error: Failed to set delete time in inode %d\n", inode_num);
		status = 1;
	}
	
	// set entry's inode to 0
	entry->inode=0;
	return status;
		
}

int ext2_rm(unsigned char *disk, char *path) {
	
	int status = 0;
	char* saved_path;
	// navigate uses strtok which splits string, so save path before using it
	saved_path = malloc(sizeof(char)* strlen(path));
	strncpy(saved_path, path, strlen(path));
	// Get the block from the given path
	struct ext2_dir_entry_2 *entry = navigate(disk, path);
	if (entry == NULL) {
		fprintf(stderr, "Invalid file or directory '%s'\n", saved_path);
		free(saved_path);
		return 1;
	}
	if (EXT2_IS_DIRECTORY(entry)) {
		fprintf(stderr, "ext2_rm: cannot remove '%s': Is a directory\n", saved_path);
		free(saved_path);
		return 1;
	}
	// Get parent directory to iterate through blocks and find
	// prev and curr block
	// Find last / in saved_path
	char *file_name = strrchr(saved_path,'/');
	// Set the character at this point to '\0', thus terminating the string
	*file_name = '\0';
	// increment file name so it points to whatever was after the last /
	file_name++;
	// saved_path variable is now set to the parent directory, so get parent block
	struct ext2_dir_entry_2 *parent = navigate(disk, saved_path);
	free(saved_path);
	struct ext2_inode * parent_inode = get_inode(disk, parent->inode);
	// Get previous block
	struct ext2_dir_entry_2 *prev= iterate_inode(disk, parent_inode, is_prev_directory, &(entry->name));

	if (prev == NULL) {
		fprintf(stderr, "Error: Unable to find previous directory entry");
		return 1;
	}
	// Get the inode from the block
	struct ext2_inode *inode = get_inode(disk, entry->inode);
	// decrement if hardlink
	inode->i_links_count --;
	if (inode->i_links_count <= 0) { // No other link to this file, delete inode
		status = delete_entry(disk, inode, entry);
	}
	// add to rec_len of previous dir entry so that deleted file gets skipped
	// do this in a loop to check when dir size ovetakes blocks size 
	struct ext2_dir_entry_2 *iter_entry = NULL;
	int limit = EXT2_NUM_BLOCKS(disk, parent_inode);
	// re-use prev for iteration variable	
	int *blocks = inode_to_blocks(disk, parent_inode);
	int block_size = 0;
	int finished = 0;
	int i = 0;
	// Loop through blocks
	for (i=0; i < limit && (!finished); i ++) {
		// re use prev variable as iterator
		// Get first entry in directory
		iter_entry = (struct ext2_dir_entry_2 *)EXT2_BLOCK(disk, blocks[i]);
		prev = NULL;
		// Keep track of how close we are to hitting the block size
		block_size = 0;
		// Loop through each directory entry
		while ((!finished) && block_size <= EXT2_BLOCK_SIZE) {
			// Check when we reach the wanted entry
			if (strcmp(entry->name, iter_entry->name)==0) {
				// update rec_len of prev
				if (prev != NULL) {
					prev->rec_len += iter_entry->rec_len;
				} else if (block_size + iter_entry->rec_len >= EXT2_BLOCK_SIZE) { // If the total size it great than the size of the block, clear the block ikn the parent node
					parent_inode->i_block[i] = 0;	
				} else { // Update value of next
					struct ext2_dir_entry_2 *next = EXT2_NEXT_FILE(iter_entry);
					next->rec_len -= iter_entry->rec_len;
				}
				finished = 1;
			}
			if (!finished) {
				block_size += iter_entry->rec_len;
				prev = iter_entry;
				if (iter_entry->rec_len == 0) finished = 1;
				iter_entry = EXT2_NEXT_FILE(iter_entry);
			}
		}
	}
	free(blocks);
	return status;
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
