#include "../include/block_distributor.h"
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <gmodule.h>

size_t block_group_free(unsigned char block) {
    return 8 - __builtin_popcount(block);
}

size_t block_group_first_free(unsigned char block) {
    // Return the position of the first free block, not as efficicent but should be fast enough
    for (int cur = 7; cur >= 0; cur -= 1) {
        if (((block >> (unsigned char) cur) & 1) == 0) {
            return 7 - cur;
        }
    }
    return 8;
}

void reinit_free_start(struct block_distributor* bd) {
    // TODO: Refresh the complete list of free start blocks
    bd->num_free_start = 0;
    printf("REINIT\n");
    for (size_t idx = 0; idx < 32; idx += 1) {
        size_t cur = rand() % 412878;
        if (block_group_free(bd->block_groups[cur])) {
            bd->free_start[bd->num_free_start++] = cur * 8 + block_group_first_free(bd->block_groups[cur]);
        }
    }
    printf("Found %zu values\n", bd->num_free_start);
    return;
}

void block_distributor_init(struct block_distributor* bd) {
    reinit_free_start(bd);
    memset(&bd->block_groups, (char) 0, 412878);
}

size_t get_block_group(size_t block) {
    return block / (size_t) 8;
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

int try_block_append(struct block_distributor* bd, struct fs_node* node, size_t first_free, size_t req_blocks) {
    size_t group_no = get_block_group(first_free);
    size_t locl_pos = first_free % 8;
    size_t tail_length = 8 - locl_pos;
    if (!block_group_free_tail(bd->block_groups[group_no], tail_length)) {
        return -1;
    }
    if (req_blocks < tail_length) {
        unsigned char pattern = ((unsigned char) 255 << (8 - req_blocks)) >> locl_pos;
        bd->block_groups[group_no] |= pattern;
    } else {
        size_t rem = req_blocks - tail_length;
        size_t view = group_no;
        // printf("Continuing storage, searching for %zu blocks\n", req_blocks);
        while (rem > 0) {
            view += 1;
            // TODO: Check for end of memory
            if (rem >= 8 && block_group_free(bd->block_groups[view]) != 8) {
                // printf("Not enough free!\n");
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
        bd->block_groups[group_no] |= ((unsigned char) 255 >> (8 - tail_length));
        for (size_t blocks = group_no + 1; blocks < view; blocks += 1) {
            bd->block_groups[blocks] = (unsigned char) 0;
        }
        bd->block_groups[view] |= ((unsigned char) 255 << (8-rem));
    }
    // Update node pointer
    struct block_pointer* bp = g_list_last(node->bps)->data;
    bp->block_length += req_blocks;
    // printf("Extending block pointer from %zu to %zu\n", bp->block_begin, bp->block_length);
    // struct block_pointer* local_p = malloc(sizeof(struct block_pointer));
    // local_p->block_begin = first_free;
    // local_p->block_length = req_blocks;
    // node->bps = g_list_append(node->bps, local_p);
    node->meta.st_blocks += req_blocks;

    size_t next = bp->block_begin + bp->block_length;
    for (size_t cur = 0; cur < bd->num_free_start; cur += 1) {
        if (bd->free_start[cur] > bp->block_begin && bd->free_start[cur] < bp->block_begin + bp->block_length) {
            if ((bd->block_groups[get_block_group(next)] >> (7 - (next % 8)) & 1) != 1) {
                bd->free_start[0] = next;
            } else {
                for (size_t nxt = cur+1; nxt < bd->num_free_start; nxt += 1) {
                    bd->free_start[nxt-1] = bd->free_start[nxt];
                }
                bd->num_free_start -= 1;
            }
        }
    }

    // bd->num_free_start = 0;

    return req_blocks;
}

int fetch_block_and_append(struct block_distributor* bd, struct fs_node* node, size_t size, size_t req_blocks) {
    //printf("Fetching %zu blocks with %zu free starts at %zu\n", req_blocks, bd->num_free_start, bd->free_start[0]);
    if (bd->num_free_start <= 0) {
        // Free Block Range Failed...
        // printf("REINIT\n");
        reinit_free_start(bd);
    }
    size_t foo = rand();

    for (size_t idx = 0; idx < bd->num_free_start; idx += 1) {
        size_t cur = (foo + idx) % bd->num_free_start;
        size_t first_free = bd->free_start[cur];
        size_t group_no = get_block_group(first_free);

        printf("Testing group %zu at position %zu\n", group_no, first_free);
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
            if (block_group_free(bd->block_groups[group_no]) > 0) {
                bd->free_start[cur] = group_no * 8 + block_group_first_free(bd->block_groups[group_no]);
                // printf("Set next free to %zu\n", bd->free_start[cur]);
            } else {
                size_t next = group_no + 1;
                // TODO: Check end of blocks
                if (block_group_free(bd->block_groups[next]) > 0) {
                    bd->free_start[cur] = next * 8 + block_group_first_free(bd->block_groups[next]);
                    // printf("Set next free to %zu\n", bd->free_start[cur]);
                }
            }
            return req_blocks;
        } else if (
            block_group_free_tail(bd->block_groups[group_no], block_group_free(bd->block_groups[group_no]))
        ) {
            // printf("Try block append.\n");
            if (try_block_append(bd, node, first_free, req_blocks) >= 0) {
                return req_blocks;
            }
        } else {
            continue;
        }
    }


    printf("Unsuccesfull Allocation.\n");
    reinit_free_start(bd);
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
    // printf("REALLOC\n");
    int present_blocks = node->meta.st_blocks;
    req_blocks -= present_blocks;

    // TODO: Shrinking of allocated space not supported
    // printf("CHECK EMPTY\n");
    if (req_blocks <= 0) {
        return 0;
    }

    // printf("CHECK EMPTY\n");
    if (node->bps == NULL) {
        // printf("Empty block pointers, allocating...\n");
        return block_distributor_alloc(bd, node, size);
    }

    // Last known space
    struct block_pointer* last = (struct block_pointer*) g_list_last(node->bps)->data;
    size_t continuation_block = last->block_begin + last->block_length;

    // printf("TRY APPEND\n");
    if (try_block_append(bd, node, continuation_block, req_blocks) >= 0) {
        return req_blocks;
    }
    // printf("Continuation not possible, searching new range\n");
    return fetch_block_and_append(bd, node, size, req_blocks);
}

size_t min(size_t a, size_t b) {
    if (a < b) {
        return a;
    }
    return b;
}

int block_distributor_free(struct block_distributor* bd, struct fs_node* node) {
    printf("Freeing storage.\n");
    GList* cur;
    for (cur = node->bps; cur != NULL; cur = cur->next) {
        struct block_pointer* bp = cur->data;
        size_t group = get_block_group(bp->block_begin);
        size_t start_length = min(8 - (bp->block_begin % 8), bp->block_length);
        bd->block_groups[group] ^= (unsigned char) 255 << (8 - start_length) >> bp->block_begin % 8;
        if (start_length >= bp->block_length) {
            return 0;
        }
        size_t rem = bp->block_begin - start_length;
        while (rem >= 8) {
            bd->block_groups[++group] = 0;
            rem -= 8;
        }
        bd->block_groups[++group] ^= (unsigned char) 255 << (8 - rem);
    }
    return 0;
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
