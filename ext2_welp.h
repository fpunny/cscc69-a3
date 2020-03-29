#include <sys/mman.h>
#include <assert.h>
#include <string.h>
#include <fcntl.h>
#include "ext2.h"

/* Helpful Notes
    entry: ext2_dir_entry_2
*/

#define EXT2_GROUP_DESC(disk) ((struct ext2_group_desc *)(disk + (EXT2_BLOCK_SIZE * 2)))
#define EXT2_SUPER_BLOCK(disk) ((struct ext2_super_block *)(disk + EXT2_BLOCK_SIZE))

// https://www.nongnu.org/ext2-doc/ext2.html#i-blocks
#define EXT2_NUM_BLOCKS(disk, entry) (entry->i_blocks/(2 << (EXT2_SUPER_BLOCK(disk)->s_log_block_size)))
#define EXT2_NEXT_FILE(entry) ((struct ext2_dir_entry_2 *)((char *)entry + entry->rec_len))
#define EXT2_BLOCK(disk, x) (disk + (EXT2_BLOCK_SIZE * x))
#define EXT2_DIRECT_BLOCKS 12

// Type checks
#define EXT2_IS_DIRECTORY(entry) ((entry != NULL) && (entry->file_type == EXT2_FT_DIR))
#define EXT2_IS_LINK(entry) ((entry != NULL) && (entry->file_type == EXT2_FT_SYMLINK))
#define EXT2_IS_FILE(entry) ((entry != NULL) && (entry->file_type == EXT2_FT_FILE))

// Helpful stuff
#define MIN(x, y) (x > y ? y : x)
#define MAX(x, y) (x < y ? y : x)

/*
 * Read image from image file
 */
unsigned char *read_image(char *image) {
    int fd = open(image, O_RDWR);
    unsigned char *disk = mmap(NULL, 128 * EXT2_BLOCK_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    return disk;
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
            map[index / 8] |= (1 << index & 8);
            (*count)--;
        } else {
            map[index / 8] &= ~(1 << index % 8);
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

int _find_file(struct ext2_dir_entry_2 *block, void *name) {
    return strcmp(block->name, (char *)name);
}
/*
 * Get directory from entry that matches name
 */
struct ext2_dir_entry_2 *find_file(unsigned char *disk, struct ext2_inode *entry, char *name) {
    return iterate_inode(disk, entry, _find_file, name);
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
