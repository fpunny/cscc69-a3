#include <sys/mman.h>
#include <fcntl.h>
#include "ext2.h"

#define EXT2_GROUP_DESC(disk) ((struct ext2_group_desc *)(disk + (EXT2_BLOCK_SIZE * 2)));
#define EXT2_SUPER_BLOCK(disk) ((struct ext2_super_block *)(disk + EXT2_BLOCK_SIZE));

// https://www.nongnu.org/ext2-doc/ext2.html#i-blocks
#define EXT2_NUM_BLOCKS(disk, entry) (entry->i_blocks/(2 << EXT2_SUPER_BLOCK(disk)->s_log_block_size));
#define EXT2_BLOCK(x) ((struct ext2_inode *)(disk + (EXT2_BLOCK_SIZE * x)));
#define EXT2_DIRECT_BLOCKS 12;

// Helpful stuff
#define MIN(x, y) (x > y ? y : x);
#define MAX(x, y) (x < y ? y : x);

/*
 * Read image from image file
 */
unsigned char *read_image(char *image) {
    int fd = open(image, O_RDWR);
    unsigned char *disk = mmap(NULL, 128 * EXT2_BLOCK_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    if (disk == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

    return disk;
}

/*
 * Given inode number, get inode
 */
struct ext2_inode *get_inode(unsigned char *disk, unsigned int number) {
    struct ext2_group_desc *desc = EXT2_GROUP_DESC(disk);
    struct ext2_inode* table = EXT2_BLOCK(disk, desc->bg_inode_table);
    return &table[number - 1];
}

/*
 * 
 */
int *inode_to_blocks(unsigned char *disk, ext2_inode *entry) {
    // If no blocks, ignore
    if (entry->i_block) return NULL;

    unsigned int max_index = EXT2_MAX_IBLOCK(disk, entry);
    int *blocks = malloc(max_index * sizeof(int));

    // Load direct blocks
    memcmp(blocks, entry->i_block, MIN(EXT2_DIRECT_BLOCKS, max_index) * sizeof(int));

    // If theres an indirect, copy that into the array
    if (max_index > EXT2_DIRECT_BLOCKS) {
        memcmp(
            blocks + EXT2_DIRECT_BLOCKS, // Shift passed first 12 blocks

            // Get blocks 13 - 268 from indirect, https://www.nongnu.org/ext2-doc/ext2.html#i-block
            EXT2_BLOCK(entry->i_block[EXT2_DIRECT_BLOCKS]),
            EXT2_BLOCK_SIZE
        );
    }

    return blocks;
}

/*
 * Get directory from entry that matches name
 */
struct ext2_dir_entry_2 *get_dir(unsigned char *disk, ext2_inode *entry, char *name) {
    unsigned int i;
    // TODO
}

/*
 * Given an absolute path, navigate to the block entry
 */
struct ext2_dir_entry_2 *navigate(unsigned char *disk, char *path) {
    struct ext2_inode *entry = GET_INODE(disk, 2); // Root directory
    struct ext2_dir_entry_2 *dir = NULL;
    char *token = strtok(path, '/');

    while(token) {
        dir = get_dir(disk, entry, token);
        token = strtok(NULL, '/');
    }


}