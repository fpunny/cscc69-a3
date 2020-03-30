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

// Helpful stuff
#define MULTIPLE_OF_FOUR(x) ((x) + ((4 - ((x)%4)) % 4))
#define MIN(x, y) (x > y ? y : x)
#define MAX(x, y) (x < y ? y : x)

// Scalars
#define EXT2_DIR_SIZE(name) MULTIPLE_OF_FOUR(sizeof(struct ext2_dir_entry_2) + strlen(name)*sizeof(char))
#define EXT2_DIRECT_BLOCKS 12

// https://www.nongnu.org/ext2-doc/ext2.html#i-blocks
#define EXT2_NUM_BLOCKS(disk, entry) (entry->i_blocks/(2 << (EXT2_SUPER_BLOCK(disk)->s_log_block_size)))
#define EXT2_NEXT_FILE(entry) ((struct ext2_dir_entry_2 *)((char *)entry + entry->rec_len))
#define EXT2_BLOCK(disk, x) (disk + (EXT2_BLOCK_SIZE * x))
#define EXT2_SET_BLOCKS(entry, x) (entry->i_blocks = (x) * (2 << (EXT2_SUPER_BLOCK(disk)->s_log_block_size)))
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
        exit(1);
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
int get_free_thing(unsigned char *disk, unsigned short limit, unsigned char *map, unsigned int start) {
    unsigned int i;
    // Go through the bitmap
    for (i = start; i < limit; i++) {
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
    unsigned int count = EXT2_SUPER_BLOCK(disk)->s_blocks_count;
    return get_free_thing(disk, count, bitmap, 0);
}

/*
 * Get the first free inode
 */
int get_free_inode(unsigned char *disk) {
    struct ext2_group_desc *desc = EXT2_GROUP_DESC(disk);
    unsigned char *bitmap = EXT2_BLOCK(disk, desc->bg_inode_bitmap);
    unsigned int count = EXT2_SUPER_BLOCK(disk)->s_inodes_count;
    return get_free_thing(disk, count, bitmap, EXT2_GOOD_OLD_FIRST_INO);
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
struct ext2_dir_entry_2 *add_thing(unsigned char *disk, struct ext2_dir_entry_2 *dir, char *name, unsigned int type) {
    // Create new file entry struct
    unsigned int required = EXT2_DIR_SIZE(name);
    struct ext2_dir_entry_2 *new_entry = (struct ext2_dir_entry_2 *)malloc(EXT2_DIR_SIZE(name));

    // Set the fields and bitmap
    strncpy(new_entry->name, name, strlen(name));
    new_entry->inode = get_free_inode(disk);
    new_entry->name_len = strlen(name);
    new_entry->file_type = type;
    set_inode_bitmap(disk, new_entry->inode, 1);

    // Find last file in directory
    struct ext2_dir_entry_2 *last_entry = NULL;
    iterate_inode(disk, get_inode(disk, dir->inode), _last_file, &last_entry);
    char *dir_name = get_name(last_entry);
    unsigned int actual = EXT2_DIR_SIZE(dir_name);
    free(dir_name);

    // We have space, add to block
    if (required <= EXT2_BLOCK_SIZE - actual) {
        new_entry->rec_len = last_entry->rec_len - actual;
        memcpy((char *)last_entry + actual, new_entry, required);
        last_entry->rec_len = actual;

        // Return new file
        free(new_entry);
        return EXT2_NEXT_FILE(last_entry);

    // No space, new block
    } else {
        new_entry->rec_len = EXT2_BLOCK_SIZE;
        struct ext2_inode *dir_inode = get_inode(disk, dir->inode);
        int index = EXT2_NUM_BLOCKS(disk, dir_inode);

        // Setup new block with entry to put into directory
        int block_index = get_free_block(disk);
        unsigned char *block = EXT2_BLOCK(disk, block_index);
        memcpy(block, new_entry, required);

        // Add direct block
        if (index < EXT2_DIRECT_BLOCKS) {
            EXT2_SET_BLOCKS(dir_inode, index + 1);
            dir_inode->i_block[index] = block_index;

        // New indirect block
        } else if (index == EXT2_DIRECT_BLOCKS) {
            // Make indirect block
            int indirect = get_free_block(disk);
            set_block_bitmap(disk, indirect, 1);
            unsigned char *indirect_block = EXT2_BLOCK(disk, indirect);
            memset(indirect_block, '\0', EXT2_BLOCK_SIZE);

            // Add indirect block and entry to directory
            EXT2_SET_BLOCKS(dir_inode, EXT2_DIRECT_BLOCKS + 1);
            dir_inode->i_block[index] = indirect;
            indirect_block[0] = block_index;

        // Add new block to indirect block
        } else {
            // Find free spot
            unsigned char *indirect_block = EXT2_BLOCK(disk, dir_inode->i_block[EXT2_DIRECT_BLOCKS]);
            int i = 0;
            while(indirect_block[i] && i < EXT2_BLOCK_SIZE/sizeof(int)) {
                i++;
            }
            if (i >= EXT2_BLOCK_SIZE) {
                printf("No space in directory\n");
                exit(1);
            }

            // Add new block to indirect
            indirect_block[i] = block_index;
        }

        free(new_entry);
        return (struct ext2_dir_entry_2 *)block;
    }

    free(new_entry);
    return NULL;
}

/*
 * Given an absolute path, navigate to the block entry
 */
struct ext2_dir_entry_2 *navigate(unsigned char *disk, char *path) {
    struct ext2_inode *inode = get_inode(disk, EXT2_ROOT_INO); // Root directory
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
