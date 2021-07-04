#ifndef BLOCK_DISTRIBUTOR_H
#define BLOCK_DISTRIBUTOR_H

#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include "fs_node.h"

#define BLOCK_SIZE (size_t) (4 * 1024)
#define FS_SIZE (size_t) (16 * 1024 * 1024 * 1024)

#define HEADER (size_t) (512)
#define INODES (size_t) (0 + HEADER)
#define ENTRIES (size_t) (64 * 1024 * 1024 + HEADER)
#define DIST_HEADER (size_t) (128 * 1024 * 1024 + HEADER)
#define DIST_CONTENT (size_t) (129 * 1024 * 1024 + HEADER)
#define CONTENT (size_t) (130 * 1024 * 1024 + HEADER)

struct block_pointer {
    size_t block_begin;
    size_t block_length;
};

struct block_distributor {
    size_t num_free_start;
    size_t free_start[32];
    unsigned char block_groups[412878];
};

void block_distributor_init(struct block_distributor* bd);

int block_distributor_alloc(struct block_distributor* bd, struct fs_node* node, size_t size);

int block_distributor_realloc(struct block_distributor* bd, struct fs_node* node, size_t size);

int block_distributor_free(struct block_distributor* bd, struct fs_node* node);

// SIMPLER METADATA BLOCKS
struct meta_block_distributor {
    unsigned char block_groups[2048];
};

void meta_block_distributor_init(struct meta_block_distributor* bd);

long meta_block_distributor_get_next_free(struct meta_block_distributor* bd);

void meta_block_distributor_free(struct meta_block_distributor* bd, size_t block);

#endif /* BLOCK_DISTRIBUTOR_H */
