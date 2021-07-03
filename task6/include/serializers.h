#ifndef SERIALIZERS_H
#define SERIALIZERS_H

#include "block_distributor.h"
#include "fs_node.h"
#include "dummyfs.h"
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <gmodule.h>

#define INODES sizeof(char) * 0
#define ENTRIES sizeof(char) * 64 * 1024 * 1024
#define DIST_HEADER sizeof(char) * 128 * 1024 * 1024
#define DIST_CONTENT sizeof(char) *129 * 1024 * 1024
#define CONTENT sizeof(char) *130 * 1024 * 1024

struct inode_writer {
    size_t offset;
    void* data;
};

inline size_t offset_from_block(size_t block, size_t offset, size_t block_size) {
    return offset + (block * block_size);
}

int serialize_entry(struct dummyfs* fs, struct fs_node* node, size_t block) {
    // TODO: Check for end block

    size_t offset = 0;
    size_t max_size = (4 * 1024 * 1024) - sizeof(size_t);
    char* dat = (char*) fs->data;

    // WRITE LENGTH NAME
    size_t len_name = strlen(node->name);
    size_t write_size = sizeof(size_t);
    memcpy(dat + offset_from_block(block, ENTRIES, BLOCK_SIZE) + offset, &len_name, write_size);
    offset += write_size;

    // WRITE NAME
    write_size = len_name;
    memcpy(dat + offset_from_block(block, ENTRIES, BLOCK_SIZE) + offset, node->name, write_size);
    offset += write_size;


    // WRITE OUT CHILDREN
    if (node->children != NULL) {
        write_size = sizeof(size_t);
        size_t len = g_list_length(node->children);
        memcpy(dat + offset_from_block(block, ENTRIES, BLOCK_SIZE) + offset, &len, write_size);
        offset += write_size;

        GList *cur;
        for (cur = node->children; cur != NULL; cur = cur->next) {
            write_size = sizeof(inode);
            inode* ino = (inode*) cur->data;
            memcpy(dat + offset_from_block(block, ENTRIES, BLOCK_SIZE) + offset, ino, write_size);
            offset += write_size;
        }
    } else {
        write_size = sizeof(size_t);
        size_t len = 0;
        memcpy(dat + offset_from_block(block, ENTRIES, BLOCK_SIZE) + offset, &len, write_size);
        offset += write_size;
    }

    // WRITE INODES
    write_size = sizeof(inode);
    memcpy(dat + offset_from_block(block, ENTRIES, BLOCK_SIZE) + offset, &node->inode, write_size);
    offset += write_size;

    // WRITE METADATA
    write_size = sizeof(struct stat);
    memcpy(dat + offset_from_block(block, ENTRIES, BLOCK_SIZE) + offset, &node->meta, write_size);
    offset += write_size;

    // WRITE BLOCKS
    if (node->bps != NULL) {
        write_size = sizeof(size_t);
        size_t len = g_list_length(node->bps);
        memcpy(dat + offset_from_block(block, ENTRIES, BLOCK_SIZE) + offset, &len, write_size);
        offset += write_size;

        GList *cur;
        for (cur = node->bps; cur != NULL; cur = cur->next) {
            write_size = sizeof(struct block_pointer);
            struct block_pointer* bp = (struct block_pointer*) cur->data;
            memcpy(dat + offset_from_block(block, ENTRIES, BLOCK_SIZE) + offset, bp, write_size);
            offset += write_size;
        }
    } else {
        write_size = sizeof(size_t);
        size_t len = 0;
        memcpy(dat + offset_from_block(block, ENTRIES, BLOCK_SIZE) + offset, &len, write_size);
        offset += write_size;
    }

    return 0;
}

void cb_serialize_inode(void* key, void* value, void* data) {
    char* path = (char*) key;
    struct inode_info* info = (struct inode_info*) value;
    struct inode_writer* writer = (struct inode_writer*) data;
    char* dat = (char*) writer->data;

    size_t path_len = strlen(path);
    memcpy(dat + writer->offset, &path_len, sizeof(size_t));
    writer->offset += sizeof(size_t);
    memcpy(dat + writer->offset, path, path_len);
    writer->offset += path_len;
    memcpy(dat + writer->offset, &info->inode, sizeof(inode));
    writer->offset += sizeof(inode);
    memcpy(dat + writer->offset, &info->block, sizeof(size_t));
    writer->offset += sizeof(size_t);
}

int serialize_inodes(struct dummyfs* fs) {
    struct inode_writer writer;
    writer.offset = 0;
    writer.data = fs->data;
    swisstable_map_foreach_sideeffect(fs->inode_map, cb_serialize_inode, (void*) &writer);
    return 0;
}

#endif /* SERIALIZERS_H */
