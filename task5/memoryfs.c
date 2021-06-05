#define _POSIX_C_SOURCE 200809L
#define FUSE_USE_VERSION 31

#include <fuse.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include "swisstable/swisstable.h"
#include <stdatomic.h>

#define TEN_MIB sizeof(char) * 10 * 1024 * 1024
#define ONE_KB sizeof(char) * 1 * 1024

typedef unsigned int inode;

struct dummyfs {
	// We use this later as we need to remove files again and reassign inodes again
	// For now this is not implemented
	inode cur_inode;
	swisstablemap_t* entry_map;
	swisstablemap_t* inode_map;
	atomic_size_t total_bytes;
};

struct fs_node {
	const char* name;
	struct fs_node** children;
	size_t num_children;
	char* content;
	size_t allocated_size;
	inode inode;
	struct stat stat;
};

inode* dummyfs_inode_add(struct dummyfs* fs, const char* name) {
	inode* n_inode = malloc(sizeof(inode));
	*n_inode = fs->cur_inode++;
	char* key = malloc(strlen(name) + 1);
	strcpy(key, name);
	swisstable_map_insert(fs->inode_map, key, strlen(key), n_inode);
	return n_inode;
}

inode* dummyfs_inode_search(struct dummyfs* fs, const char* name) {
	return swisstable_map_search(fs->inode_map, name, strlen(name));
}

void* dummyfs_entry_add(struct dummyfs* fs, inode* inode, struct fs_node* entry) {
	return swisstable_map_insert(fs->entry_map, inode, sizeof(inode), entry);
}

struct fs_node* dummyfs_entry_search(struct dummyfs* fs, inode* inode) {
	return swisstable_map_search(fs->entry_map, inode, sizeof(inode));
}

struct fs_node* dummyfs_add_file(struct dummyfs* fs, const char* name, const char* parent, struct fuse_file_info* fi, uid_t uid, gid_t gid) {
	// TODO: Add implementation to add file to the tree
	(void) fi;

	inode* p_inode = dummyfs_inode_search(fs, parent);
	struct fs_node* p_entry = dummyfs_entry_search(fs, p_inode);

	char path[strlen(name) + strlen(parent) + 1];
	strcpy(path, parent);
	if (strcmp(parent, "/") != 0) {
		strcat(path, "/");
	}
	strcat(path, name);
	inode* new_inode = dummyfs_inode_add(fs, path);
	struct fs_node* new_entry = malloc(sizeof(struct fs_node));
	// Init new fsnode
	new_entry->name = strcpy(malloc(strlen(name) + 1), name);
	new_entry->num_children = 0;
	new_entry->children = NULL;
	new_entry->content = malloc(ONE_KB);
	new_entry->allocated_size = ONE_KB;
	new_entry->inode = fs->cur_inode;

	// Init metadata
	struct stat* meta = &new_entry->stat;
	meta->st_ino = fs->cur_inode;
	meta->st_uid = uid;
	meta->st_gid = gid;
	meta->st_size = 0;
	meta->st_blocks = 1;
	meta->st_blksize = ONE_KB;
	meta->st_dev = 1337;
	meta->st_mode = S_IFREG;
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	meta->st_atime = ts.tv_sec;
	meta->st_mtime = ts.tv_sec;
	meta->st_ctime = ts.tv_sec;
	meta->st_nlink = 1;


	// Add new entry to exisiting structures
	dummyfs_entry_add(fs, new_inode, new_entry);
	p_entry->children[p_entry->num_children] = new_entry;
	p_entry->num_children += 1;

	return new_entry;
}

void dummy_add_directory(struct dummyfs* fs, const char* name, const char* parent, struct fuse_file_info* fi) {
	// TODO: Add impl for directories
	(void) fs;
	(void) name;
	(void) parent;
	(void) fi;

	if (parent == NULL) {
		// Create root dir
	} else {
		// Create regular directory
	}
}

void dummyfs_init (struct dummyfs* fs) {
	// Current maximum of 2^12 entries in the fs, for testing enough...
	// fs->inodes = calloc(4096, sizeof(unsigned int));
	fs->cur_inode = 0;
	fs->entry_map = swisstable_map_create();
	swisstable_map_reserve(fs->entry_map, 1024);
	fs->inode_map = swisstable_map_create();
	swisstable_map_reserve(fs->inode_map, 1024);
	fs->total_bytes = 0;

	inode* new_inode = dummyfs_inode_add(fs, "/");
	struct fs_node* root = malloc(sizeof(struct fs_node));
	root->name = "/";
	root->children = malloc(sizeof(struct fs_node*) * 64);
	root->num_children = 0;
	root->inode = *new_inode;
	struct stat* meta = &root->stat;
	meta->st_nlink = 2;
	meta->st_mode = S_IFDIR | 0777;
	meta->st_ino = *new_inode;
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	meta->st_atime = ts.tv_sec;
	meta->st_mtime = ts.tv_sec;
	meta->st_ctime = ts.tv_sec;

	printf("Entry inode %p added to map with %p\n", dummyfs_entry_add(fs, new_inode, root), new_inode);
	// inode* test = dummyfs_inode_search(fs, "/");
	// printf("The respective inode for %s is %u while new inode was %u\n", "/", *test, *new_inode);
	// printf("Fetched again with %u this results in pointer %p\n", *test, dummyfs_entry_search(fs, test));

	// Add Dummy File for now
	dummyfs_add_file(fs, "matrix.out", "/", NULL, 1000, 1000);
	dummyfs_add_file(fs, "hello", "/", NULL, 1000, 1000);
}

/*
** Fetch the first part of the given string until / seperator is met
** If no separator is found the whole string is returned
*/
void get_first_element(const char* source, char* target) {
	if (strlen(source) == 1 && source[0] == '/') {
		target[0] = '/';
		target[1] = '\0';
		return;
	}
	const char* full = source + 1;
	for (size_t idx = 0; idx < strlen(full); idx += 1) {
		if (full[idx] == '/') {
			break;
		}
		target[idx] = full[idx];
		target[idx+1] = '\0';
	}
}

// struct fs_node* _dummyfs_find_node(struct dummyfs* fs, const char* path, struct fs_node* parent) {
// 	struct fs_node* target = NULL;
// 	char next_child[256] = {0};
// 	get_first_element(path, next_child);
// 	// //printf("path = %s\n", path);
// 	// //printf("next_child = %s\n", next_child);
// 	if (strcmp(next_child, "/") == 0 || strcmp(path, "") == 0) {
// 		// //printf("Found %s\n", parent->name);
// 		return parent;
// 	}
// 	for (size_t idx = 0; idx < parent->num_children; idx += 1) {
// 		/// //printf("Checking %s\n", parent->children[idx].name);
// 		if (strcmp(parent->children[idx].name,next_child) == 0) {
// 			return _dummyfs_find_node(fs, path + strlen(next_child) + 1, &parent->children[idx]);
// 		}
// 	}
// 	return target;
// }
//
// struct fs_node* dummyfs_find_node(struct dummyfs* fs, const char* path) {
// 	return _dummyfs_find_node(fs, path, fs->root);
// }

static int
dummyfs_chmod (const char* path, mode_t mode, struct fuse_file_info* fi)
{
	(void) path;
	(void) mode;
	(void) fi;
	return 0;
}

static int
dummyfs_chown (const char* path, uid_t uid, gid_t gid, struct fuse_file_info* fi)
{
	(void) path;
	(void) uid;
	(void) gid;
	(void) fi;
	return 0;
}

void get_parent_path(const char* path, char* target, char* name) {
	size_t last = strlen(path);
	for (size_t idx = strlen(path) - 1; 1; idx -= 1) {
		if (path[idx] == '/') {
			last = idx + 1;
			break;
		}
	}
	strncpy(target, path, last);
	strcpy(name, path + last);
}

static int
dummyfs_create (const char* path, mode_t mode, struct fuse_file_info* fi)
{
	//printf("Entering create for path: %s\n", path);
	int res = 0;
	struct fuse_context* ctx = fuse_get_context();
	struct dummyfs* fs = (struct dummyfs*) ctx->private_data;

	if (dummyfs_inode_search(fs, path) != NULL) {
		return -EEXIST;
	}

	char parent_path[strlen(path)];
	char name[256];
	get_parent_path(path, parent_path, name);
	inode* p_inode = dummyfs_inode_search(fs, parent_path);
	if (p_inode == NULL) {
		res = -ENOENT;
		return res;
	}
	//printf("PARENT PATH FOR %s IS %s\n", path, parent_path);
	struct fs_node* new_entry = dummyfs_add_file(fs, name, parent_path, fi, ctx->uid, ctx->gid);
	new_entry->stat.st_mode = mode;

	return res;
}

static int
dummyfs_getattr (const char* path, struct stat* stbuf, struct fuse_file_info* fi)
{
	//printf("Entering getattr for path: %s\n", path);
	(void) fi;
	int res = 0;
	struct fuse_context* ctx = fuse_get_context();
	struct dummyfs* fs = (struct dummyfs*) ctx->private_data;

	inode* n_inode = dummyfs_inode_search(fs, path);
	if (n_inode == NULL) {
		res = -ENOENT;
		return res;
	}
	struct fs_node* node = dummyfs_entry_search(fs, n_inode);
	memset(stbuf, 0, sizeof(struct stat));

	if (node != NULL) {
		//printf("GETATTR from %s\n", node->name);
		// Write to target memory
		*stbuf = node->stat;
	} else {
		res = -ENOENT;
	}
	return res;
}

static int
dummyfs_link (const char* from, const char* to)
{
	(void) from;
	(void) to;
	return 0;
}

static int
dummyfs_mkdir(const char* path, mode_t mode) {
	(void) path;
	(void) mode;
	return 0;
}

static int
dummyfs_open (const char* path, struct fuse_file_info* fi)
{
	(void) path;
	(void) fi;
	return 0;
}

static int
dummyfs_read (const char* path, char* buf, size_t size, off_t offset, struct fuse_file_info* fi)
{
	(void) fi;
	//printf("Entering read for %s\n", path);
	struct fuse_context* ctx = fuse_get_context();
	struct dummyfs* fs = (struct dummyfs*) ctx->private_data;
	int res = 0;

	inode* n_inode = dummyfs_inode_search(fs, path);
	if (n_inode == NULL) {
		res = -ENOENT;
		return res;
	}
	struct fs_node* node = dummyfs_entry_search(fs, n_inode);

	if (node != NULL && node->stat.st_nlink > 0 && (node->stat.st_mode & S_IFREG) == S_IFREG) {
		//printf("READING from %s\n", node->name);
		// Write to target memory
		memcpy(buf, node->content + offset, size);
		// Update fs tree
		struct timespec ts;
		clock_gettime(CLOCK_REALTIME, &ts);
		node->stat.st_atime = ts.tv_sec;
		res = size;
	} else if (node == NULL) {
		res = -ENOENT;
	} else {
		// invalid type
		res = -ENOENT;
	}

	return res;
}

static int
dummyfs_readdir (const char* path, void* buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* fi, enum fuse_readdir_flags flags)
{
	(void) flags;
	(void) fi;
	(void) offset;
	//printf("Entering readdir\n");
	struct fuse_context* ctx = fuse_get_context();
	struct dummyfs* fs = (struct dummyfs*) ctx->private_data;

	// Act from root
	int res = 0;

	inode* n_inode = dummyfs_inode_search(fs, path);
	if (n_inode == NULL) {
		res = -ENOENT;
		return res;
	}
	struct fs_node* node = dummyfs_entry_search(fs, n_inode);

	if (node != NULL && (node->stat.st_mode & S_IFDIR) == S_IFDIR) {
		//printf("READDIR from %s\n", node->name);
		filler(buf, ".", NULL, 0, 0);
		filler(buf, "..", NULL, 0, 0);
		for (size_t idx = 0; idx < node->num_children; idx += 1) {
			if (node->children[idx]->stat.st_nlink > 0) {
				filler(buf, node->children[idx]->name, NULL, 0, 0);
			}
		}
	} else if (node == NULL) {
		res = -ENOENT;
	} else {
		// invalid type
		res = -EINVAL;
	}
	return res;
}

static int
dummyfs_rmdir (const char* path)
{
	(void) path;
	return 0;
}

static int
dummyfs_statfs (const char* path, struct statvfs* stbuf)
{
	(void) path;
	(void) stbuf;
	return 0;
}

static int
dummyfs_truncate (const char* path, off_t size, struct fuse_file_info* fi)
{
	(void) fi;
	//printf("Entering truncate for path %s (%ld bytes)\n", path, size);
	struct fuse_context* ctx = fuse_get_context();
	struct dummyfs* fs = (struct dummyfs*) ctx->private_data;
	int res = 0;

	inode* n_inode = dummyfs_inode_search(fs, path);
	if (n_inode == NULL) {
		res = -ENOENT;
		return res;
	}
	struct fs_node* node = dummyfs_entry_search(fs, n_inode);

	if (node != NULL && (node->stat.st_mode & S_IFREG) == S_IFREG) {
		if (node->stat.st_size < size) {
			return -1;
		}
		node->stat.st_size = size;
		//printf("Setting size to %ld", node->stat.st_size);
	} else if (node != NULL) {
		res = -EINVAL;
	} else {
		res = -ENOENT;
	}

	return res;
}

static int
dummyfs_unlink (const char* path)
{
	//printf("Entering unlink for path %s\n", path);
	int res = 0;
	struct fuse_context* ctx = fuse_get_context();
	struct dummyfs* fs = (struct dummyfs*) ctx->private_data;

	char parent_path[256];
	char name[256];
	get_parent_path(path, parent_path, name);

	inode* n_inode = dummyfs_inode_search(fs, path);
	if (n_inode == NULL) {
		res = -ENOENT;
		return res;
	}
	struct fs_node* node = dummyfs_entry_search(fs, n_inode);

	if (node != NULL) {
		node->stat.st_nlink = 0;
	}

	return res;
}

static int
dummyfs_utimens (const char* path, const struct timespec ts[2], struct fuse_file_info* fi)
{
	(void) path;
	(void) ts;
	(void) fi;
	return 0;
}

static int
dummyfs_write (const char* path, const char* buf, size_t size, off_t offset, struct fuse_file_info* fi)
{
	(void) fi;
	//printf("Entering write for path %s\n", path);
	struct fuse_context* ctx = fuse_get_context();
	struct dummyfs* fs = (struct dummyfs*) ctx->private_data;
	int res = 0;

	inode* inode = dummyfs_inode_search(fs, path);
	struct fs_node* node = dummyfs_entry_search(fs, inode);
	if (node != NULL && node->stat.st_nlink > 0) {
		// Write to target memory
		memcpy(node->content + offset, buf, size);
		// Update fs tree
		if ((off_t)(offset + size) > node->stat.st_size) {
			node->stat.st_size += size - (node->stat.st_size - offset);
			//printf("Increasing size to %ld\n", node->stat.st_size);
		}
		struct timespec ts;
		clock_gettime(CLOCK_REALTIME, &ts);
		node->stat.st_mtime = ts.tv_sec;
		res = size;
	} else if (node == NULL) {
		res = -ENOENT;
	} else {
		// invalid type
		res = -EISDIR;
	}

	return res;
}

struct fuse_operations dummyfs_oper = {
	.chmod    = dummyfs_chmod,
	.chown    = dummyfs_chown,
	.create   = dummyfs_create,
	.getattr  = dummyfs_getattr,
	.link     = dummyfs_link,
	.mkdir    = dummyfs_mkdir,
	.open     = dummyfs_open,
	.read     = dummyfs_read,
	.readdir  = dummyfs_readdir,
	.rmdir    = dummyfs_rmdir,
	.statfs   = dummyfs_statfs,
	.truncate = dummyfs_truncate,
	.unlink   = dummyfs_unlink,
	.utimens  = dummyfs_utimens,
	.write    = dummyfs_write,
};

int main (int argc, char* argv[])
{
	struct dummyfs dfs;
	dummyfs_init(&dfs);

	//printf("Initialized!\n");
	return fuse_main(argc, argv, &dummyfs_oper, &dfs);
}
