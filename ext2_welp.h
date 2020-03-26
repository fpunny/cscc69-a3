#include <sys/mman.h>
#include <string.h>
#include <fcntl.h>
#include "ext2.h"

#define EXT2_GROUP_DESC(disk) ((struct ext2_group_desc *)(disk + (EXT2_BLOCK_SIZE * 2)))
#define EXT2_SUPER_BLOCK(disk) ((struct ext2_super_block *)(disk + EXT2_BLOCK_SIZE))

// https://www.nongnu.org/ext2-doc/ext2.html#i-blocks
#define EXT2_NUM_BLOCKS(disk, entry) (entry->i_blocks/(2 << (EXT2_SUPER_BLOCK(disk)->s_log_block_size)))
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
 * 
 */
int *inode_to_blocks(unsigned char *disk, struct ext2_inode *entry) {
    // If no blocks, ignore
    if (!entry->i_block) return NULL;

    unsigned int limit = EXT2_NUM_BLOCKS(disk, entry);
    printf("%d\n", limit);
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
 * Get directory from entry that matches name
 */
struct ext2_dir_entry_2 *get_block(unsigned char *disk, struct ext2_inode *entry, char *name) {
    int *blocks = inode_to_blocks(disk, entry);
    int limit = EXT2_NUM_BLOCKS(disk, entry);
    struct ext2_dir_entry_2 *res = NULL;
    struct ext2_dir_entry_2 *block;
    unsigned int i = 0;

    while (i < limit && !res) {
        block = (struct ext2_dir_entry_2 *)EXT2_BLOCK(disk, blocks[i]);
        if (strcmp(block->name, name) == 0) {
            res = block;
        }
        i++;
    }

    free(blocks);
    return res;
}

/*
 * Given an absolute path, navigate to the block entry
 */
struct ext2_dir_entry_2 *navigate(unsigned char *disk, char *path) {
    struct ext2_inode *inode = get_inode(disk, 2); // Root directory
    struct ext2_dir_entry_2 *entry = get_block(disk, inode, ".");
    char *token = strtok(path, "/");

    while(token) {
        entry = get_block(disk, inode, token);
        token = strtok(NULL, "/");

        // If subdirectory is a file, return NULL
        if (!EXT2_IS_DIRECTORY(entry) && token) {
            return NULL;
        }
        inode = get_inode(disk, entry->inode);
    }

    // Return the file/directory
    return entry;
}