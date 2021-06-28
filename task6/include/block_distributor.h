#ifndef BLOCK_DISTRIBUTOR_H
#define BLOCK_DISTRIBUTOR_H

#include <stddef.h>
#include <string.h>
#include "fs_node.h"

#define BLOCK_SIZE sizeof(char) * 4 * 1024

struct block_pointer {
    size_t block_begin;
    size_t block_length;
};

struct block_distributor {
    size_t num_free_start;
    size_t free_start[32];
    char block_groups[412878];
};

void block_distributor_init(struct block_distributor* bd) {
    bd->num_free_start = 0;
    memset(&bd->block_groups, (char) 0, 412878);
}

int block_distributor_alloc(struct block_distributor* bd, struct fs_node* node, size_t size);

int block_distributor_realloc(struct block_distributor* bd, struct fs_node* node, size_t size);

int block_distributor_free(struct block_distributor* bd, struct fs_node* node);

#endif /* BLOCK_DISTRIBUTOR_H */
