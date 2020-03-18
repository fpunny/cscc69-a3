
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "ext2.h"

unsigned char *disk;

struct dir{
    unsigned int block_no;
    unsigned int inode_no;
   struct ext2_dir_entry_2 *d;
   struct dir * next;
   struct ext2_inode *in;
};

struct dir *head;
struct dir *last;
int main(int argc, char **argv) {

    if(argc != 2) {
        fprintf(stderr, "Usage: %s <image file name>\n", argv[0]);
        exit(1);
    }
    int fd = open(argv[1], O_RDWR);

    disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(disk == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

    struct ext2_super_block *sb = (struct ext2_super_block *)(disk + 1024);
    struct ext2_group_desc *blgrp = (struct ext2_group_desc *)( disk + 2048);




    printf("Inodes: %d\n", sb->s_inodes_count);
    printf("Blocks: %d\n", sb->s_blocks_count);
    printf("Block group:\n");
    printf("   block bitmap: %d\n", blgrp->bg_block_bitmap);
    printf("   inode bitmap: %d\n", blgrp->bg_inode_bitmap);
    printf("   inode table: %d\n", blgrp->bg_inode_table);
    printf("   free blocks: %d\n", blgrp->bg_free_blocks_count);
    printf("   free inodes: %d\n", blgrp->bg_free_inodes_count);
    printf("   used dirs: %d\n", blgrp->bg_used_dirs_count);
    

    //ex 8 

    int size =  sb->s_blocks_count/ 8;
    printf("Block bitmap:");
    unsigned char * bm = ( unsigned char * )(disk + EXT2_BLOCK_SIZE  * blgrp->bg_block_bitmap);
    int i,x;    
    unsigned char buf ;
    for (i = 0; i < size; i++) {
      printf(" ");
      buf = *bm;
      for (x = 0; x < 8; x++) {
        printf("%d", (buf >> x) & 1);
    }
    bm++;
    }
    printf("\n");


    printf("Inode bitmap:");
    size =  sb->s_inodes_count/ 8;
    unsigned char * im = ( unsigned char * )(disk + EXT2_BLOCK_SIZE  * blgrp->bg_inode_bitmap);    
    for (i = 0; i < size; i++) {
      printf(" ");
      buf = *im;
      for (x = 0; x < 8; x++) {
        printf("%d", (buf >> x) & 1);
    }
    im++;
    }
    // set dir head to null
    head = NULL;

    printf("\n\n");
    printf("Inodes:\n");
    unsigned char  *itable = (unsigned char *)(disk + EXT2_BLOCK_SIZE * blgrp->bg_inode_table);
    char type = '0';

    for ( i = 1; i < sb->s_inodes_count; i++){
        if( i > 2 && i < EXT2_GOOD_OLD_FIRST_INO){
            continue;
        }
        struct ext2_inode * inode = (struct ext2_inode *) ( itable + sizeof(struct ext2_inode) * i);
        if(inode->i_size == 0){
            continue;
        }

        if ( inode->i_mode & EXT2_S_IFREG){
            type = 'f';       
        }else{
            type = 'd';
            if ( head == NULL){
                head = malloc(sizeof(struct dir));
                head->next = NULL;
                head->in = inode;
                head->inode_no = i;
                last = head;
            }
            else{
                last->next = malloc(sizeof(struct dir));
                last = last->next;
                last->next = NULL;
                last->in = inode;
                last->inode_no = i;  
        
            }
        };
        printf("[%d] type: %c size: %d links: %d blocks: %d\n", i + 1, type, inode->i_size, inode->i_links_count, inode->i_blocks);
        int j;
        printf("[%d] Blocks: ", i + 1);
        int num_blocks = inode->i_blocks / 2;
        for (j = 0; j < num_blocks; j++) {
            if (j >= 12) {
               break;
            }
            if(type == 'd'){
                last->block_no = inode->i_block[j];
            }
         printf(" %d\n", inode->i_block[j]);
        }

    }



    // ex9
    printf("\n");
    printf("Directory Blocks:\n");

    while(head != NULL){
      
       printf("   DIR BLOCK NUM: %d (for inode %d)\n", head->block_no, head->inode_no +1);
       head->d = (struct ext2_dir_entry_2 *)(disk + EXT2_BLOCK_SIZE * head->block_no);  

        int i = 0;
           while (i < EXT2_BLOCK_SIZE) {
              i += head->d->rec_len;
             type = '0';
             if ((unsigned int) head->d->file_type == EXT2_FT_REG_FILE) {
              type = 'f';
                } 
             else if((unsigned int) head->d->file_type == EXT2_FT_DIR) {
               type = 'd';
             }
             printf("Inode: %d rec_len: %d name_len: %d type= %c name=%.*s\n", 
             head->d->inode, head->d->rec_len, head->d->name_len, type, head->d->name_len, head->d->name);
             head->d = (void *) head->d + head->d->rec_len;
          }
        struct dir *f = head;
        head = head->next;
        free(f);
    }
    
    return 0;
}
