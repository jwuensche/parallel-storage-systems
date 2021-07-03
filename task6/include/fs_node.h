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
	size_t allocated_size;
	inode inode;
	struct stat meta;
	GList* bps;
};

struct inode_info {
	inode inode;
	size_t block;
};

#endif /* FS_NODE_H */
