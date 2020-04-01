#include <sys/mman.h>
#include <assert.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>
#include <stdlib.h>
#include <stdint.h>
#include "ext2.h"

/* Helpful Notes
    entry: ext2_dir_entry_2
*/

#define EXT2_GROUP_DESC(disk) ((struct ext2_group_desc *)(disk + (EXT2_BLOCK_SIZE * 2)))
#define EXT2_SUPER_BLOCK(disk) ((struct ext2_super_block *)(disk + EXT2_BLOCK_SIZE))

// Helpful stuff
#define MULTIPLE_OF_FOUR(x) (x + ((4 - (x%4)) % 4))
#define MIN(x, y) (x > y ? y : x)
#define MAX(x, y) (x < y ? y : x)

// Scalars
#define EXT2_DIR_SIZE(name) MULTIPLE_OF_FOUR(sizeof(struct ext2_dir_entry_2) + strlen(name)*sizeof(char))
#define EXT2_DIRECT_BLOCKS 12

// https://www.nongnu.org/ext2-doc/ext2.html#i-blocks
#define EXT2_NUM_BLOCKS(disk, entry) (entry->i_blocks/(2 << (EXT2_SUPER_BLOCK(disk)->s_log_block_size)))
#define EXT2_NEXT_FILE(entry) ((struct ext2_dir_entry_2 *)((char *)entry + entry->rec_len))
#define EXT2_BLOCK(disk, x) (disk + (EXT2_BLOCK_SIZE * x))
#define SET_BIT_1(map, index) (map[index / 8] |= (1 << index & 8))
#define SET_BIT_0(map, index) (map[index / 8] &= ~(1 << index % 8))

// Type checks
#define EXT2_IS_DIRECTORY(entry) ((entry != NULL) && (entry->file_type == EXT2_FT_DIR))
#define EXT2_IS_FILE(entry) ((entry != NULL) && (entry->file_type == EXT2_FT_REG_FILE))
#define EXT2_IS_LINK(entry) ((entry != NULL) && (entry->file_type == EXT2_FT_SYMLINK))

/*
 * Read image from image file
 */
unsigned char *read_image(char *image) {
    int fd = open(image, O_RDWR);
    unsigned char *disk = mmap(NULL, 128 * EXT2_BLOCK_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    return disk;
}

char *get_name(struct ext2_dir_entry_2 *entry) {
    char *name = (char *)malloc((entry->name_len + 1) * sizeof(char));
    strncpy(name, entry->name, entry->name_len);
    name[entry->name_len] = '\0';
    return name;
}

/*
 * Given inode number, get inode
 */
struct ext2_inode *get_inode(unsigned char *disk, unsigned int number) {
    struct ext2_group_desc *desc = EXT2_GROUP_DESC(disk);
    struct ext2_inode *table = (struct ext2_inode *)EXT2_BLOCK(disk, desc->bg_inode_table);
    return &table[number - 1];
}

/*
 * Gets the blocks with corespond to the inode
 */
int *inode_to_blocks(unsigned char *disk, struct ext2_inode *entry) {
    // If no blocks, ignore
    if (!entry->i_block) return NULL;

    unsigned int limit = EXT2_NUM_BLOCKS(disk, entry);
    int *blocks = malloc(limit * sizeof(int));
    assert(blocks);

    // Load direct blocks
    memcpy(blocks, entry->i_block, MIN(EXT2_DIRECT_BLOCKS, limit) * sizeof(int));

    // If theres an indirect, copy that into the array
    if (limit > EXT2_DIRECT_BLOCKS) {
        memcpy(
            blocks + EXT2_DIRECT_BLOCKS, // Shift passed first 12 blocks

            // Get blocks 13 - 268 from indirect, https://www.nongnu.org/ext2-doc/ext2.html#i-block
            EXT2_BLOCK(disk, entry->i_block[EXT2_DIRECT_BLOCKS]),
            EXT2_BLOCK_SIZE
        );
    }

    return blocks;
}

/*
 * Sets bit, and count, for bitmap of a thing
 */
int set_thing_bitmap(unsigned char *disk, unsigned int index, unsigned state, unsigned char *map, unsigned short *count) {
    unsigned bit = map[index / 8] & (1 << index % 8);
    if (state && !*count) {
        perror("bitmap");
        return 1;
    }

    if (bit != state) {
        if (state) {
            SET_BIT_1(map, index);
            (*count)--;
        } else {
            SET_BIT_0(map, index);
            (*count)++;
        }
    }

    return 0;
}

/*
 * Sets bit, and count, for block bitmap
 */
int set_block_bitmap(unsigned char *disk, unsigned int index, unsigned state) {
    struct ext2_group_desc *desc = EXT2_GROUP_DESC(disk);
    unsigned char *bitmap = EXT2_BLOCK(disk, desc->bg_block_bitmap);
    unsigned short count = desc->bg_free_blocks_count;
    return set_thing_bitmap(disk, index, state, bitmap, &count);
}

/*
 * Sets bit, and count, for inode bitmap
 */
int set_inode_bitmap(unsigned char *disk, unsigned int index, unsigned state) {
    struct ext2_group_desc *desc = EXT2_GROUP_DESC(disk);
    unsigned char *bitmap = EXT2_BLOCK(disk, desc->bg_inode_bitmap);
    unsigned short count = desc->bg_free_inodes_count;
    return set_thing_bitmap(disk, index, state, bitmap, &count);
}

/*
 * Searches provided bitmap for first free space 
 */
int get_free_thing(unsigned char *disk, unsigned short limit, unsigned char *map) {
    unsigned int i;
    // Go through the bitmap
    for (i = 0; i < limit; i++) {
        unsigned bit = map[i/8];

        // If it's free, then return the inode
        if ((bit & (1 << i%8)) == 0) {
            return i;
        }
    }

    return -1;
}

/*
 * get the first free block
 */
int get_free_block(unsigned char *disk) {
    struct ext2_group_desc *desc = EXT2_GROUP_DESC(disk);
    unsigned char *bitmap = EXT2_BLOCK(disk, desc->bg_block_bitmap);
    return get_free_thing(disk, EXT2_SUPER_BLOCK(disk)->s_blocks_count, bitmap);
}

/*
 * Get the first free inode
 */
int get_free_inode(unsigned char *disk) {
    struct ext2_group_desc *desc = EXT2_GROUP_DESC(disk);
    unsigned char *bitmap = EXT2_BLOCK(disk, desc->bg_inode_bitmap);
    return get_free_thing(disk, EXT2_SUPER_BLOCK(disk)->s_inodes_count, bitmap);
}

/*
 * Go over all blocks, passing them one by one into the callback. If callback return 0,
 * then return block. else keep going. Your welcome...
 */
struct ext2_dir_entry_2 *iterate_inode(
    unsigned char *disk,
    struct ext2_inode *entry,
    int (*callback)(struct ext2_dir_entry_2 *, void *),
    void *params
) {
    int *blocks = inode_to_blocks(disk, entry);
    int limit = EXT2_NUM_BLOCKS(disk, entry);
    struct ext2_dir_entry_2 *block;
    int i, j;

    // Loop through blocks
    for (i = 0; i < limit; i++) {
        block = (struct ext2_dir_entry_2 *)EXT2_BLOCK(disk, blocks[i]);

        // Loop through entries in block
        for (j = 0; j < EXT2_BLOCK_SIZE; (j += block->rec_len, block = EXT2_NEXT_FILE(block))) {
            if ((*callback)(block, params) == 0) {
                free(blocks);
                return block;
            }
        }
    }

    free(blocks);
    return NULL;
}

int _find_file(struct ext2_dir_entry_2 *block, void *name) {
    char *dir_name = get_name(block);
    int res = strcmp(dir_name, (char *)name);
    free(dir_name);
    return res;
}
/*
 * Get directory from entry that matches name
 */
struct ext2_dir_entry_2 *find_file(unsigned char *disk, struct ext2_inode *entry, char *name) {
    return iterate_inode(disk, entry, _find_file, name);
}

int _last_file(struct ext2_dir_entry_2 *block, void *last) {
    *(struct ext2_dir_entry_2 **)last = block;
    return 1;
}
/* WIP */ 
struct ext2_dir_entry_2 *add_file(unsigned char *disk, struct ext2_dir_entry_2 *dir, char *name) {
    // Create new file entry struct
    unsigned int required = EXT2_DIR_SIZE(name);
    struct ext2_dir_entry_2 *new_entry = (struct ext2_dir_entry_2 *)malloc(EXT2_DIR_SIZE(name));

    // Set the fields and bitmap
    new_entry->inode = get_free_inode(disk);
    new_entry->file_type = EXT2_FT_REG_FILE;
    new_entry->name_len = strlen(name);
    set_inode_bitmap(disk, new_entry->inode, 1);

    // Find last file in directory
    struct ext2_dir_entry_2 *last_entry = NULL;
    iterate_inode(disk, get_inode(disk, dir->inode), _last_file, &last_entry);
    unsigned int actual = EXT2_DIR_SIZE(last_entry->name);

    // We have space, add to block
    if (required <= EXT2_BLOCK_SIZE - actual) {
        new_entry->rec_len = last_entry->rec_len - actual;
        memcpy(last_entry + actual, new_entry, new_entry->rec_len);
        last_entry->rec_len = actual;

        // Return new file
        free(new_entry);
        return EXT2_NEXT_FILE(last_entry);

    // No space, new block
    } else {
        struct ext2_inode *dir_inode = get_inode(disk, dir->inode);
        int block_index = get_free_block(disk);
        int index = EXT2_NUM_BLOCKS(disk, dir_inode);
        printf("%d %d\n", block_index, index);
    }

    free(new_entry);
    return NULL;
}

/*
 * Given an absolute path, navigate to the block entry
 */
struct ext2_dir_entry_2 *navigate(unsigned char *disk, char *path) {
    struct ext2_inode *inode = get_inode(disk, 2); // Root directory
    struct ext2_dir_entry_2 *entry = find_file(disk, inode, ".");
    
    char *token = strtok(path, "/");
    while(token) {
        entry = find_file(disk, inode, token);
        token = strtok(NULL, "/");
        // If subdirectory is a file, return NULL. Else return the last file
        if (!EXT2_IS_DIRECTORY(entry)) {
            return token ? NULL : entry;
        }
        inode = get_inode(disk, entry->inode);
    }
    
    // Return the file/directory
    return entry;
}

/*
 *
 */
struct ext2_dir_entry_2 *get_parent(unsigned char *disk, char *path) {
	// Get parent directory to iterate through blocks and find
	char* saved_path = malloc(sizeof(char)* strlen(path));
        strncpy(saved_path, path, strlen(path));
	struct ext2_dir_entry_2 *entry = NULL;
	
	// Find last / in saved_path
	char *file_name = strrchr(saved_path,'/');
	if (file_name == NULL) {// in root directory
		entry = navigate(disk, "/"); // get root entry
	} else {
		// Set the character at this point to '\0', thus terminating the string
		*file_name = '\0';
		// saved_path variable is now set to the parent directory, so get parent block
		entry = navigate(disk, saved_path);
	}
	free(saved_path);
	return entry;
}

/*
 * Given an inode, remove an inode.
 */
int remove_inode(unsigned char *disk, struct ext2_dir_entry_2 *entry) {

	int status = 0;
	int inode_num = entry->inode;
	struct ext2_inode *inode = get_inode(disk, inode_num);
	
	// Get the super block
	struct ext2_super_block *super = EXT2_SUPER_BLOCK(disk);
	// Get the block group
	struct ext2_group_desc *blgrp = EXT2_GROUP_DESC(disk);

	unsigned char *inode_bm = ( unsigned char * )(disk + EXT2_BLOCK_SIZE  * blgrp->bg_inode_bitmap);
	int block_pos = inode_num / 8;
	int bit_pos = (inode_num % 8) -1;
	int byte = inode_bm[block_pos];

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

/*
 * Given an entry and an inode, clear the info for the bitmaps and set appropriate fields
 */
int delete_entry(unsigned char* disk, struct ext2_inode *inode, struct ext2_dir_entry_2 * entry) {

	int i;
	unsigned int num_blocks;
	int *blocks;
	int block_pos;
	int bit_pos;
	int byte;
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
	
	remove_inode(disk, entry);
	return status;		
}

/*
 * Update the rec_len of the prev file, this removing the given entry from the directory
 */
int update_rec_len(unsigned char *disk, struct ext2_dir_entry_2 *entry, char *path) {
	int status = 0;
	
	struct ext2_dir_entry_2 *parent = get_parent(disk, path);
	struct ext2_inode * parent_inode = get_inode(disk, parent->inode);

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
		struct ext2_dir_entry_2 *prev = NULL;
		// Keep track of how close we are to hitting the block size
		block_size = 0;
		// Loop through each directory entry
		while ((!finished) && block_size <= EXT2_BLOCK_SIZE) {
			// Check when we reach the wanted entry
			if (strcmp(get_name(entry), get_name(iter_entry))==0) {
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


/*
 * Given the disk and an absolute path to an entry, update entry fields to simulate file deletion.
 * This says file but to avoid refactoring issues, it allows the deletion of links, files, and directories
 */
int remove_file(unsigned char *disk, char *path) {

	int status = 0;
	char* saved_path = malloc(sizeof(char)* strlen(path));
        strncpy(saved_path, path, strlen(path));
	// get actual file that needs to be removed
	struct ext2_dir_entry_2 *entry = navigate(disk, saved_path);
	// update rec len to show removal
	update_rec_len(disk, entry, path);
	free(saved_path);
	if (EXT2_IS_LINK(entry)) {
		// Remove inode
		status = remove_inode(disk, entry);
	} else {
		// Get the inode from the block
		struct ext2_inode *inode = get_inode(disk, entry->inode);

		if (EXT2_IS_DIRECTORY(entry)) {
			// update_rec_len removes file from linked list in parent directory, so decrement link count
			inode->i_links_count --;
		}

		// decrement if hardlink
		inode->i_links_count --;
		if (inode->i_links_count <= 0) { // No other link to this, delete entry (which deletes inode)
			if (EXT2_IS_FILE(entry) || EXT2_IS_DIRECTORY(entry)) {
				delete_entry(disk, inode, entry);
			}
		}
	}

	return status;
}



