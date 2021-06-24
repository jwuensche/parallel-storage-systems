#ifndef FS_NODE_H
#define FS_NODE_H

#include <fcntl.h>
#include <stdatomic.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <gmodule.h>

typedef unsigned int inode;

struct fs_node {
	// A poor mans spinlock, this is better substituted with another explicit structure but time is running low...
	atomic_flag busy;
	char* name;
	size_t num_children;
	GList* children;
	char* content;
	inode inode;
	struct stat meta;
	size_t num_blocks;
	struct block_pointer* bps;
};

#endif /* FS_NODE_H */
