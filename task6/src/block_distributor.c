#include "../include/block_distributor.h"
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <gmodule.h>

void block_distributor_init(struct block_distributor* bd) {
    bd->num_free_start = 1;
    bd->free_start[0] = 0;
    memset(&bd->block_groups, (char) 0, 412878);
}

size_t block_group_free(unsigned char block) {
    return 8 - __builtin_popcount(block);
}

size_t get_block_group(size_t block) {
    return block / (size_t) 8;
}

int reinit_free_start(struct block_distributor* bd) {
    // TODO: Refresh the complete list of free start blocks
    return 0;
}

// Return value represents the length free blocks in the range specified
int block_group_free_tail(unsigned char block, size_t req) {
    return (((unsigned char) 255 >> (8-req)) & block) == 0;
}

// Return value represents the length free blocks in the range specified
size_t block_group_free_head(unsigned char block, size_t req) {
    return block_group_free(((unsigned char) 255 << (char) req) & block);
}

int block_group_seq(unsigned char block, size_t req) {
    char pattern = 255 >> (8 - req);
    return
        ((block >> 0) & pattern) == 0 ||
        ((block >> 1) & pattern) == 0 ||
        ((block >> 2) & pattern) == 0 ||
        ((block >> 3) & pattern) == 0 ||
        ((block >> 4) & pattern) == 0 ||
        ((block >> 5) & pattern) == 0 ||
        ((block >> 6) & pattern) == 0 ||
        ((block >> 7) & pattern) == 0;
}

int block_group_first_free(unsigned char block) {
    // Return the position of the first free block, not as efficicent but should be fast enough
    for (int cur = 7; cur >= 0; cur -= 1) {
        if (((block >> (unsigned char) cur) & 1) == 0) {
            return 7 - cur;
        }
    }
    return -1;
}

int try_block_append(struct block_distributor* bd, struct fs_node* node, size_t first_free, size_t group_no, size_t req_blocks) {
        size_t first = block_group_free_tail(bd->block_groups[group_no], block_group_free(bd->block_groups[group_no]));
        // printf("First blocks: %zu\n", first);
        size_t rem = req_blocks - first;
        size_t view = group_no;
        // printf("Continuing storage, searching for %zu blocks\n", req_blocks);
        while (rem > 0) {
            view += 1;
            // TODO: Check for end of memory
            if (rem >= 8 && block_group_free(bd->block_groups[view]) != 8) {
                printf("Not enough free!\n");
                return -1;
            }
            if (rem <= 8 && block_group_free_head(bd->block_groups[view], rem) == rem) {
                // Success!
                break;
            }
            if (rem > 8 && block_group_free(bd->block_groups[view]) == 8) {
                rem -= 8;
                continue;
            }
            return -1;
        }
        // Search was successful and view contains the last block with have to consider
        bd->block_groups[group_no] |= ((unsigned char) 255 >> (8 - first));
        for (size_t blocks = group_no + 1; blocks < view; blocks += 1) {
            bd->block_groups[blocks] = (unsigned char) 0;
        }
        bd->block_groups[view] |= ((unsigned char) 255 << rem);
        // Update node pointer
        struct block_pointer* bp = g_list_last(node->bps)->data;
        bp->block_length += req_blocks;
        // printf("Extending block pointer from %zu to %zu\n", bp->block_begin, bp->block_length);
        // struct block_pointer* local_p = malloc(sizeof(struct block_pointer));
        // local_p->block_begin = first_free;
        // local_p->block_length = req_blocks;
        // node->bps = g_list_append(node->bps, local_p);
        node->meta.st_blocks += req_blocks;

        return 0;
}

int fetch_block_and_append(struct block_distributor* bd, struct fs_node* node, size_t size, size_t req_blocks) {
    if (bd->num_free_start <= 0 || reinit_free_start(bd) < 0) {
        // Free Block Range Failed...
    }

    // printf("Fetching block\n");
    for (size_t cur = 0; cur < bd->num_free_start; cur += 1) {
        size_t first_free = bd->free_start[cur];
        size_t group_no = get_block_group(first_free);

        // printf("Testing group %zu at position %zu\n", group_no, first_free);
        // printf("Free in Block group: %zu\n", block_group_free(bd->block_groups[group_no]));
        // printf("Sequential blocks free: %d\n", block_group_seq(bd->block_groups[group_no], req_blocks));

        if (
            block_group_free(bd->block_groups[group_no]) >= req_blocks &&
            block_group_seq(bd->block_groups[group_no], req_blocks)
        ) {
            // Update Storage bitmap
            char block_pos = first_free % 8;
            char pattern = (255 >> (8 - req_blocks)) << (8 - block_pos - req_blocks);
            bd->block_groups[group_no] |= pattern;

            // Update node pointer
            // printf("Assigning block pointer from %zu over %zu\n", first_free, req_blocks);
            struct block_pointer* local_p = malloc(sizeof(struct block_pointer));
            local_p->block_begin = first_free;
            local_p->block_length = req_blocks;
            node->bps = g_list_append(node->bps, local_p);
            node->meta.st_blocks += req_blocks;

            // Update next known free
            if (block_group_first_free(bd->block_groups[group_no]) >= 0) {
                bd->free_start[cur] = group_no * 8 + block_group_first_free(bd->block_groups[group_no]);
            } else {
                // TODO: Check following block group
                size_t next = group_no + 1;
                // TODO: Check end of blocks
                if (block_group_free(bd->block_groups[next]) > 0) {
                    bd->free_start[cur] = next * 8 + block_group_first_free(bd->block_groups[next]);
                }
            }
            return req_blocks;
        } else if (
            block_group_free_tail(bd->block_groups[group_no], block_group_free(bd->block_groups[group_no])) == block_group_free(bd->block_groups[group_no])
        ) {
            // printf("Try block append.\n");
            if (try_block_append(bd, node, first_free, group_no, req_blocks) >= 0) {
                return req_blocks;
            }
        } else {
            continue;
        }
    }


    // printf("Unsuccesfull.\n");
    return -1;
}

int block_distributor_alloc(struct block_distributor* bd, struct fs_node* node, size_t size) {
    size_t req_blocks = size / (size_t) BLOCK_SIZE;
    if (size % BLOCK_SIZE > 0) {
        // Uneven split add one more block
        req_blocks += 1;
    }

    // printf("Allocating %zu blocks (per %zu bytes) of size %zu bytes \n", req_blocks, BLOCK_SIZE, size);
    return fetch_block_and_append(bd, node, size, req_blocks);
}

int block_distributor_realloc(struct block_distributor* bd, struct fs_node* node, size_t size) {
    int req_blocks = size / BLOCK_SIZE;
    if (size % BLOCK_SIZE > 0) {
        // Uneven split add one more block
        req_blocks += 1;
    }
    int present_blocks = node->meta.st_blocks;
    req_blocks -= present_blocks;

    // TODO: Shrinking of allocated space not supported
    if (req_blocks <= 0) {
        return 0;
    }

    if (node->bps == NULL) {
        return block_distributor_alloc(bd, node, size);
    }

    // Last known space
    struct block_pointer* last = (struct block_pointer*) g_list_last(node->bps)->data;
    size_t continuation_block = last->block_begin + last->block_length;
    size_t group_no = continuation_block % 8;

    if (try_block_append(bd, node, continuation_block, group_no, req_blocks) >= 0) {
        return req_blocks;
    }

    return fetch_block_and_append(bd, node, size, req_blocks);
}

int block_distributor_free(struct block_distributor* bd, struct fs_node* node) {
    // TODO: Free
    return -1;
}

void meta_block_distributor_init(struct meta_block_distributor *bd) {
    memset(&bd->block_groups, (char) 0, 2048);
}

long meta_block_distributor_get_next_free(struct meta_block_distributor *bd) {
    for (size_t cur = 0; cur < 2048; cur += 1) {
        //printf("Checking block group %zu\n", cur);
        if (bd->block_groups[cur] < 255) {
            size_t local_pos = block_group_first_free(bd->block_groups[cur]);
            bd->block_groups[cur] |= (unsigned char) 128 >> local_pos;
            //printf("Returning block %zu\n", 8 *cur + local_pos);
            return 8 * cur + local_pos;
        }
    }
    return -1;
}

void meta_block_distributor_free(struct meta_block_distributor *bd, size_t block) {
    size_t bg = block  % 8;
    bd->block_groups[bg] ^= (unsigned char) 128 >> (block / 8);
}
