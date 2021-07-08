#ifndef DERSERIALIZERS_H
#define DERSERIALIZERS_H

#include "block_distributor.h"
#include <stdatomic.h>
#include <stdlib.h>
#include "fs_node.h"
#include "dummyfs.h"
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <gmodule.h>

inline size_t offset_from_block_de(size_t block, size_t offset, size_t block_size) {
    return offset + (block * block_size);
}

void deserialize_entry(struct dummyfs* fs, size_t block, inode* g_inode) {
    // TODO: Check for end block

    size_t offset = 0;
    size_t max_size = (4 * 1024 * 1024) - sizeof(size_t);
    char* dat = (char*) fs->data;

    struct fs_node* node = (struct fs_node*) malloc(sizeof(struct fs_node));

    // READ LENGTH NAME
    size_t len_name = 0;
    size_t read_size = sizeof(size_t);
    memcpy(&len_name, dat + offset_from_block_de(block, ENTRIES, BLOCK_SIZE) + offset, read_size);
    offset += read_size;

    // READ NAME
    read_size = len_name;
    node->name = (char*) malloc(read_size);
    memcpy(node->name, dat + offset_from_block_de(block, ENTRIES, BLOCK_SIZE) + offset, read_size);
    offset += read_size;

    // READ CHILDREN NUM
    read_size = sizeof(size_t);
    memcpy(&node->num_children, dat + offset_from_block_de(block, ENTRIES, BLOCK_SIZE) + offset, read_size);
    offset += read_size;

    // READ CHILDREN
    node->children = NULL;
    for (size_t cur = 0; cur < node->num_children; cur += 1) {
        read_size = sizeof(inode);
        inode* ino = (inode*) malloc(sizeof(inode));
        memcpy(ino, dat + offset_from_block_de(block, ENTRIES, BLOCK_SIZE) + offset, read_size);
        offset += read_size;
        node->children = g_list_append(node->children, ino);
    }

    // READ INODE
    read_size = sizeof(inode);
    memcpy(&node->inode, dat + offset_from_block_de(block, ENTRIES, BLOCK_SIZE) + offset, read_size);
    offset += read_size;

    // READ METADATA
    read_size = sizeof(struct stat);
    memcpy(&node->meta, dat + offset_from_block_de(block, ENTRIES, BLOCK_SIZE) + offset, read_size);
    offset += read_size;

    // READ BLOCKS NUM
    read_size = sizeof(size_t);
    size_t len = 0;
    memcpy(&len, dat + offset_from_block_de(block, ENTRIES, BLOCK_SIZE) + offset, read_size);
    offset += read_size;

    // READ BLOCKS
    node->bps = NULL;
    for (size_t cur = 0; cur < len; cur += 1) {
        read_size = sizeof(struct block_pointer);
        struct block_pointer* bp = (struct block_pointer*) malloc(read_size);
        memcpy(bp, dat + offset_from_block_de(block, ENTRIES, BLOCK_SIZE) + offset, read_size);
        offset += read_size;
        node->bps = g_list_append(node->bps, bp);
    }

    // INITALIZE MISC
	atomic_flag flag = ATOMIC_FLAG_INIT;
    node->busy = flag;

    // ADD TO MAP
    swisstable_map_insert(fs->entry_map, g_inode, sizeof(inode), node);

    return;
}

void deserialize_inodes(struct dummyfs* fs) {
    size_t offset = INODES;
    char* dat = (char*) fs->data;

    size_t read_size = sizeof(size_t);
    memcpy(&fs->inode_count, dat + offset, read_size);
    offset += read_size;

    for (size_t cur = 0; cur < fs->inode_count; cur += 1) {
        struct inode_info* info = (struct inode_info*) malloc(sizeof(struct inode_info));

        // READ LENGTH NAME
        size_t path_len = 0;
        size_t read_size = sizeof(size_t);
        memcpy(&path_len, dat + offset, read_size);
        offset += read_size;

        // READ NAME
        char* path = (char*) malloc(path_len + 1);
        read_size = path_len;
        memcpy(path, dat + offset, read_size);
        offset += read_size;
        path[path_len] = '\0';

        // READ INODE
        read_size = sizeof(inode);
        memcpy(&info->inode, dat + offset, read_size);
        offset += read_size;

        // READ BLOCK
        read_size = sizeof(size_t);
        memcpy(&info->block, dat + offset, read_size);
        offset += read_size;

        // ADD TO MAP
        swisstable_map_insert(fs->inode_map, path, path_len, info);
    }

    return;
}

void deserialize_content_bitmap(struct dummyfs* fs) {
    memcpy(fs->block_distributor, (char*) fs->data + DIST_CONTENT, sizeof(struct block_distributor));
    return;
}

void deserialize_meta_bitmap(struct dummyfs* fs) {
    memcpy(fs->meta_distributor, (char*) fs->data + DIST_HEADER, sizeof(struct meta_block_distributor));
    return;
}


#endif /* DERSERIALIZERS_H */
