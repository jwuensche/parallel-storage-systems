#include "../include/block_distributor.h"
#include <stddef.h>
#include <stdlib.h>
#include <gmodule.h>

void block_distributor_init(struct block_distributor* bd) {
    bd->num_free_start = 0;
    memset(&bd->block_groups, (char) 0, 412878);
}

size_t block_group_free(char block) {
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
size_t block_group_free_tail(char block, size_t req) {
    return block_group_free(((char) 255 >> (8-req)) & block);
}

// Return value represents the length free blocks in the range specified
size_t block_group_free_head(char block, size_t req) {
    return block_group_free(((char) 255 << req) & block);
}

int block_group_seq(char block, size_t req) {
    char pattern = 255 >> (8 - req);
    return
        ((block >> 0) & pattern) == pattern ||
        ((block >> 1) & pattern) == pattern ||
        ((block >> 2) & pattern) == pattern ||
        ((block >> 3) & pattern) == pattern ||
        ((block >> 4) & pattern) == pattern ||
        ((block >> 5) & pattern) == pattern ||
        ((block >> 6) & pattern) == pattern ||
        ((block >> 7) & pattern) == pattern;
}

int block_group_first_free(char block) {
    // Return the position of the first free block, not as efficicent but should be fast enough
    for (size_t cur = 7; cur >= 0; cur -= 1) {
        if (block >> cur == 1) {
            return 7 - cur;
        }
    }
    return -1;
}


int try_block_append(struct block_distributor* bd, struct fs_node* node, size_t first_free, size_t group_no, size_t req_blocks) {
        size_t first = block_group_free_tail(bd->block_groups[group_no], block_group_free(bd->block_groups[group_no]));
        size_t rem = req_blocks - first;
        size_t view = group_no;
        while (rem > 0) {
            view += 1;
            // TODO: Check for end of memory
            if (block_group_free(bd->block_groups[view]) != 8 && rem > 8) {
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
        bd->block_groups[group_no] |= ((char) 255 >> (8 - first));
        for (size_t blocks = group_no + 1; blocks < view; blocks += 1) {
            bd->block_groups[blocks] = (char) 0;
        }
        bd->block_groups[view] |= ((char) 255 << rem);
        // Update node pointer
        struct block_pointer* local_p = malloc(sizeof(struct block_pointer));
        local_p->block_begin = first_free;
        local_p->block_length = req_blocks;
        node->bps = g_list_append(node->bps, local_p);

        return 0;
}

int fetch_block_and_append(struct block_distributor* bd, struct fs_node* node, size_t size, size_t req_blocks) {
    if (bd->num_free_start <= 0 || reinit_free_start(bd) < 0) {
        // Free Block Range Failed...
    }

    for (size_t cur = 0; cur < bd->num_free_start; cur += 1) {
        size_t first_free = bd->free_start[cur];
        size_t group_no = get_block_group(first_free);

        if (
            block_group_free(bd->block_groups[group_no]) >= req_blocks &&
            block_group_seq(bd->block_groups[group_no], block_group_free(bd->block_groups[group_no]))
        ) {
            // Update Storage bitmap
            char block_pos = first_free % 8;
            char pattern = (255 >> (8 - req_blocks)) << (8 - block_pos - req_blocks);
            bd->block_groups[group_no] |= pattern;

            // Update node pointer
            struct block_pointer* local_p = malloc(sizeof(struct block_pointer));
            local_p->block_begin = first_free;
            local_p->block_length = req_blocks;
            node->bps = g_list_append(node->bps, local_p);

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
            if (try_block_append(bd, node, first_free, group_no, req_blocks) >= 0) {
                return req_blocks;
            }
        } else {
            continue;
        }
    }

    return -1;
}

int block_distributor_alloc(struct block_distributor* bd, struct fs_node* node, size_t size) {
    size_t req_blocks = size / BLOCK_SIZE;
    if (size % BLOCK_SIZE > 0) {
        // Uneven split add one more block
        req_blocks += 1;
    }

    return fetch_block_and_append(bd, node, size, req_blocks);
}

int block_distributor_realloc(struct block_distributor* bd, struct fs_node* node, size_t size) {
    size_t req_blocks = size / BLOCK_SIZE;
    if (size % BLOCK_SIZE > 0) {
        // Uneven split add one more block
        req_blocks += 1;
    }
    size_t present_blocks = node->meta.st_blocks;
    req_blocks -= present_blocks;

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
