#include "string.h"
#include "fs_node.h"
#include "dummyfs.h"
#include "block_distributor.h"

size_t clamp(size_t a, size_t upper) {
    if (a > upper) {
        return upper;
    }
    return a;
}

int blockwrite(struct dummyfs* fs, const void* src, struct fs_node* node, size_t offset, size_t n) {
    size_t written_bytes = 0;
    char* dat = (char*) fs->data;
    char* sr = (char*) src;
    GList* cur;
    for (cur = node->bps; cur != NULL; cur = cur->next) {
        struct block_pointer* bp = cur->data;
        if (bp->block_length * BLOCK_SIZE <= offset) {
            offset -= bp->block_length * BLOCK_SIZE;
            continue;
        }
        size_t bytes_to_write = clamp(n, bp->block_length * BLOCK_SIZE - offset);
        memcpy(dat + CONTENT + (bp->block_begin * BLOCK_SIZE) + offset, sr + written_bytes, bytes_to_write);
        offset = 0;
        written_bytes = bytes_to_write;
        n -= bytes_to_write;
        if (n == 0) {
            return 0;
        }
    }
    return -1;
}

int blockread(struct dummyfs* fs, const void* dst, struct fs_node* node, size_t offset, size_t n) {
    size_t read_bytes = 0;
    char* dat = (char*) fs->data;
    char* ds = (char*) dst;
    GList* cur;
    for (cur = node->bps; cur != NULL; cur = cur->next) {
        struct block_pointer* bp = cur->data;
        if (bp->block_length * BLOCK_SIZE <= offset) {
            offset -= bp->block_length * BLOCK_SIZE;
            continue;
        }
        size_t bytes_to_read = clamp(n, bp->block_length * BLOCK_SIZE - offset);
        memcpy(ds + read_bytes, dat + CONTENT + (bp->block_begin * BLOCK_SIZE) + offset,  bytes_to_read);
        offset = 0;
        read_bytes = bytes_to_read;
        n -= bytes_to_read;
        if (n == 0) {
            return 0;
        }
    }
    return -1;
}
