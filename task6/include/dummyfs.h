#ifndef DUMMYFS_H
#define DUMMYFS_H

#include "../swisstable/swisstable.h"
#include <stdatomic.h>

typedef unsigned int inode;

struct dummyfs {
	void* data;
	inode cur_inode;
	swisstablemap_t* entry_map;
	swisstablemap_t* inode_map;
	atomic_size_t total_bytes;
};

#endif /* DUMMYFS_H */
