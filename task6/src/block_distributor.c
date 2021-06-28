#include "../include/block_distributor.h"
#include <stddef.h>

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

int fetch_block_and_append(struct block_distributor* bd, struct fs_node* node, size_t size, size_t req_blocks) {
    if (bd->num_free_start > 0 || reinit_free_start(bd) < 0) {
        // Free Block Range Failed...
    }

    for (size_t cur = 0; cur < bd->num_free_start; cur += 1) {
        size_t first_free = bd->free_start[cur];
        size_t group_no = get_block_group(first_free);
        size_t rem_blocks = req_blocks;

        if (
            block_group_free(bd->block_groups[group_no]) >= req_blocks &&
            block_group_seq(bd->block_groups[group_no], block_group_free(bd->block_groups[group_no]))
        ) {
            // Update Storage bitmap
            char block_pos = first_free % 8;
            char pattern = (255 >> (8 - req_blocks)) << (8 - block_pos - req_blocks);
            bd->block_groups[group_no] |= pattern;

            // Update node pointer
            struct block_pointer local_p;
            local_p.block_begin = first_free;
            local_p.block_length = req_blocks;
            node->bps[node->num_blocks] = local_p;
            node->num_blocks += 1;

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
            size_t first = block_group_free_tail(bd->block_groups[group_no], block_group_free(bd->block_groups[group_no]));
            size_t rem = req_blocks - first;
            size_t view = group_no;
            char suc = 0;
            while (rem > 0) {
                view += 1;
                // TODO: Check for end of memory
                if (block_group_free(bd->block_groups[view]) != 8 && rem > 8) {
                    goto skip;
                }
                if (rem <= 8 && block_group_free_head(bd->block_groups[view], rem) == rem) {
                    // Success!
                    break;
                }
                if (rem > 8 && block_group_free(bd->block_groups[view]) == 8) {
                    rem -= 8;
                    continue;
                }
                goto skip;
            }
            // Search was successful and view contains the last block with have to consider
            bd->block_groups[group_no] |= ((char) 255 >> (8 - first));
            for (size_t blocks = group_no + 1; blocks < view; blocks += 1) {
                bd->block_groups[blocks] = (char) 0;
            }
            bd->block_groups[view] |= ((char) 255 << rem);
            // Update node pointer
            struct block_pointer local_p;
            local_p.block_begin = first_free;
            local_p.block_length = req_blocks;
            node->bps[node->num_blocks] = local_p;
            node->num_blocks += 1;
        } else {
            skip:
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
    size_t continuation_block = node->bps[node->num_blocks-1].block_begin + node->bps[node->num_blocks-1].block_length;

    for (size_t cur = 0; cur < bd->num_block_ranges; cur +=1) {
        struct block_pointer bp = bd->block_range[cur];

        if (bp.block_begin == continuation_block && bp.block_length >= req_blocks) {
            // Fitting continuation
            node->bps[node->num_blocks-1].block_length += req_blocks;
            if (bp.block_begin > req_blocks) {
                // Remaining space after
                bp.block_length -= req_blocks;
                bp.block_begin += req_blocks;
            } else {
                // Remove logically reassign this later
                bp.block_length = 0;
            }
        }
        if (bp.block_begin > continuation_block) {
            break;
        }
    }

    return fetch_block_and_append(bd, node, size, req_blocks);
}

int block_distributor_free(struct block_distributor* bd, struct fs_node* node) {
    for (size_t cur = 0; cur < node->num_blocks; cur += 1) {
        struct block_pointer bp = node->bps[cur];
    }
    node->num_blocks = 0;
    return -1;
}
