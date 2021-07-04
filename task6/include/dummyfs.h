#ifndef DUMMYFS_H
#define DUMMYFS_H

#include "../swisstable/swisstable.h"
#include <stdatomic.h>

typedef unsigned int inode;

struct dummyfs {
	atomic_flag busy;
	void* data;
	inode cur_inode;
	swisstablemap_t* entry_map;
	swisstablemap_t* inode_map;
	struct block_distributor* block_distributor;
	struct meta_block_distributor* meta_distributor;
	atomic_size_t total_bytes;
};

#endif /* DUMMYFS_H */
