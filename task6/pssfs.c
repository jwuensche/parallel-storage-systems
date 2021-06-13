#include <asm-generic/errno-base.h>
#include <stddef.h>
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
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
#include <gmodule.h>

#define TEN_MIB sizeof(char) * 10 * 1024 * 1024
#define ONE_KB sizeof(char) * 1 * 1024
#define FOUR_GB sizeof(char) * 4 * 1024 * 1024 * 1024

typedef unsigned int inode;

struct dummyfs {
	inode cur_inode;
	swisstablemap_t* entry_map;
	swisstablemap_t* inode_map;
	atomic_size_t total_bytes;
};

struct fs_node {
	// A poor mans spinlock, this is better substituted with another explicit structure but time is running low...
	atomic_flag busy;
	char* name;
	GList* children;
	size_t num_children;
	char* content;
	size_t allocated_size;
	inode inode;
	struct stat stat;
};

void* dummyfs_allocate(struct dummyfs* fs, size_t bytes) {
	if (fs->total_bytes + bytes > FOUR_GB) {
		return NULL;
	}
	void* ptr = malloc(bytes);
	if (ptr != NULL) {
		fs->total_bytes += bytes;
	}
	return ptr;
}

void* dummyfs_reallocate(struct dummyfs*fs, void* old, size_t bytes, size_t current_bytes) {
	if (current_bytes < bytes && FOUR_GB < fs->total_bytes + (bytes - current_bytes)) {
		return NULL;
	}
	void* ptr = realloc(old, bytes);
	if (ptr != NULL) {
		// I don't wanna cast around
		if (current_bytes > bytes) {
			fs->total_bytes -= (current_bytes - bytes);
		} else {
			fs->total_bytes += (bytes - current_bytes);
		}
	}
	return ptr;
}

inode* dummyfs_inode_add(struct dummyfs* fs, const char* name) {
	inode* n_inode = dummyfs_allocate(fs, (sizeof(inode)));
	*n_inode = fs->cur_inode++;
	char* key = dummyfs_allocate(fs, (strlen(name) + 1));
	strcpy(key, name);
	// printf("Adding %s with length %lu\n", key, strlen(key));
	swisstable_map_insert(fs->inode_map, key, strlen(key), n_inode);
	return n_inode;
}

inode* dummyfs_inode_add_explicit(struct dummyfs* fs, const char* name, inode* ino) {
	char* key = dummyfs_allocate(fs, (strlen(name) + 1));
	strcpy(key, name);
	swisstable_map_insert(fs->inode_map, key, strlen(key), ino);
	return ino;
}

inode* dummyfs_inode_search(struct dummyfs* fs, const char* name) {
	// printf("Searching %s with length %lu\n", name, strlen(name));
	return swisstable_map_search(fs->inode_map, name, strlen(name));
}

void* dummyfs_entry_add(struct dummyfs* fs, inode* inode, struct fs_node* entry) {
	return swisstable_map_insert(fs->entry_map, inode, sizeof(inode), entry);
}

struct fs_node* dummyfs_entry_search(struct dummyfs* fs, inode* inode) {
	return swisstable_map_search(fs->entry_map, inode, sizeof(inode));
}

struct fs_node* dummyfs_add_file(struct dummyfs* fs, const char* name, const char* parent, struct fuse_file_info* fi, uid_t uid, gid_t gid) {
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
	struct fs_node* new_entry = dummyfs_allocate(fs, (sizeof(struct fs_node)));
	// Init new fsnode
	atomic_flag flag = ATOMIC_FLAG_INIT;
	new_entry->busy = flag;
	new_entry->name = strcpy(dummyfs_allocate(fs, (strlen(name) + 1)), name);
	new_entry->num_children = 0;
	new_entry->children = NULL;
	new_entry->content = dummyfs_allocate(fs, (ONE_KB));
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
	p_entry->children = g_list_append(p_entry->children, new_entry);
	p_entry->num_children += 1;

	return new_entry;
}

struct fs_node* dummyfs_add_directory(struct dummyfs* fs, const char* name, const char* parent, struct fuse_file_info* fi, uid_t uid, gid_t gid) {
	(void) fi;

	inode* new_inode;
	if (parent == NULL) {
		// Create root dir
		new_inode = dummyfs_inode_add(fs, name);
	} else {
		// Create regular directory
		char tmp[strlen(name) + strlen(parent) + 1];
		strcpy(tmp, parent);
		if (strcmp(parent, "/") != 0) {
			strcat(tmp, "/");
		}
		strcat(tmp, name);
		new_inode = dummyfs_inode_add(fs, tmp);
	}
	struct fs_node* dir = dummyfs_allocate(fs, (sizeof(struct fs_node)));

	atomic_flag flag = ATOMIC_FLAG_INIT;
	dir->busy = flag;
	dir->name = strcpy(dummyfs_allocate(fs, strlen(name) + 1), name);
	dir->children = NULL;
	dir->num_children = 0;
	dir->inode = *new_inode;
	struct stat* meta = &dir->stat;
	meta->st_ino = *new_inode;
	meta->st_uid = uid;
	meta->st_gid = gid;
	meta->st_nlink = 2;
	meta->st_mode = S_IFDIR | 0755;
	meta->st_blocks = 1;
	meta->st_blksize = ONE_KB;
	meta->st_dev = 1337;
	meta->st_size = sizeof(inode);
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	meta->st_atime = ts.tv_sec;
	meta->st_mtime = ts.tv_sec;
	meta->st_ctime = ts.tv_sec;

	dummyfs_entry_add(fs, new_inode, dir);

	if (parent != NULL) {
		inode* p_inode = dummyfs_inode_search(fs, parent);
		struct fs_node* p_entry = dummyfs_entry_search(fs, p_inode);
		p_entry->children = g_list_append(p_entry->children, dir);
	}

	return dir;
}

void dummyfs_init (struct dummyfs* fs) {
	fs->cur_inode = 0;
	fs->total_bytes = 0;
	fs->entry_map = swisstable_map_create();
	swisstable_map_reserve(fs->entry_map, 1024);
	fs->total_bytes += 1024 * sizeof(char*);
	fs->total_bytes += 1024 * sizeof(inode*);
	fs->inode_map = swisstable_map_create();
	swisstable_map_reserve(fs->inode_map, 1024);
	fs->total_bytes += 1024 * sizeof(char*);
	fs->total_bytes += 1024 * sizeof(struct fs_node*);

	// inode* test = dummyfs_inode_search(fs, "/");
	// printf("The respective inode for %s is %u while new inode was %u\n", "/", *test, *new_inode);
	// printf("Fetched again with %u this results in pointer %p\n", *test, dummyfs_entry_search(fs, test));
	//
	dummyfs_add_directory(fs, "/", NULL, NULL, 1000, 1000);
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
	if (last == 1) {
		strncpy(target, path, 1);
		target[1] = '\0';
	} else {
		strncpy(target, path, last - 1);
		target[last-1] = '\0';
	}
	strcpy(name, path + last);
}

static int
dummyfs_create (const char* path, mode_t mode, struct fuse_file_info* fi)
{
	//printf("Enterin
	int res = 0;
	struct fuse_context* ctx = fuse_get_context();
	struct dummyfs* fs = (struct dummyfs*) ctx->private_data;

	if (dummyfs_inode_search(fs, path) != NULL) {
		return -EEXIST;
	}

	char parent_path[strlen(path)];
	char name[strlen(path)];
	get_parent_path(path, parent_path, name);
	//printf("PARENT PATH FOR %s IS %s\n", path, parent_path);
	inode* p_inode = dummyfs_inode_search(fs, parent_path);
	if (p_inode == NULL) {
		res = -ENOENT;
		return res;
	}
	struct fs_node* p_entry = dummyfs_entry_search(fs, p_inode);
	while (atomic_flag_test_and_set(&p_entry->busy)) {}
	struct fs_node* new_entry = dummyfs_add_file(fs, name, parent_path, fi, ctx->uid, ctx->gid);
	new_entry->stat.st_mode = mode;
	atomic_flag_clear(&p_entry->busy);

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
		// printf("GETATTR from %s\n", node->name);
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
	struct fuse_context* ctx = fuse_get_context();
	struct dummyfs* fs = (struct dummyfs*) ctx->private_data;
	int res = 0;

	if (dummyfs_inode_search(fs, path) != NULL) {
		res = -EEXIST;
		return res;
	}
	char p_path[strlen(path)];
	char d_name[strlen(path)];
	get_parent_path(path, p_path, d_name);
	inode* p_inode = dummyfs_inode_search(fs, p_path);
	// printf("Looking up parent \"%s\" with %lu\n", p_path, strlen(p_path));
	if (p_inode == NULL) {
		res = -ENOTDIR;
		return res;
	}
	struct fs_node* p_entry = dummyfs_entry_search(fs, p_inode);
	if ((p_entry->stat.st_mode & S_IFDIR) == S_IFDIR) {
		while (atomic_flag_test_and_set(&p_entry->busy)) {}
		struct fs_node* d_node = dummyfs_add_directory(fs, d_name, p_path, NULL, ctx->uid, ctx->gid);
		d_node->stat.st_mode = mode | S_IFDIR;
		atomic_flag_clear(&p_entry->busy);
	} else {
		res = -EINVAL;
	}

	// if (dummyfs_inode_search(fs, path) == NULL) {
	// 	printf("Tried to add %s with length %lu but it was not there\n", path, strlen(path));
	// 	exit(69);
	// }
	return res;
}

static int
dummyfs_open (const char* path, struct fuse_file_info* fi)
{
	(void)fi;
	struct fuse_context* ctx = fuse_get_context();
	struct dummyfs* fs = (struct dummyfs*) ctx->private_data;
	int res = 0;

	inode* n_inode = dummyfs_inode_search(fs, path);
	if (n_inode == NULL) {
		res = -ENOENT;
		return res;
	}
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
		// printf("Accessing %zu bytes at %p with offset %ld .\n", size, node->content, offset);
		// printf("Reported size is %ld and allocated %zu\n", node->stat.st_size, node->allocated_size);
		while (atomic_flag_test_and_set(&node->busy)) {}
		if (offset + size > (unsigned long) node->stat.st_size) {
			memcpy(buf, node->content + offset, node->stat.st_size - offset);
		} else {
			memcpy(buf, node->content + offset, size);
		}
		atomic_flag_clear(&node->busy);
		// printf("Accessed content.\n");
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

void dummyfs_rename_subdirs(struct dummyfs* fs, struct fs_node* o_entry, const char* old, const char* new) {
	if ((o_entry->stat.st_mode & S_IFDIR) == S_IFDIR && o_entry->children != NULL) {
		GList* elem;
		for (elem = o_entry->children; elem != NULL; elem = elem->next) {
			struct fs_node* child = elem->data;
			char oc_path[strlen(old) + strlen(child->name) + 1];
			strcpy(oc_path, old);
			strcat(oc_path, "/");
			strcat(oc_path, child->name);
			char nc_path[strlen(new) + strlen(child->name) + 1];
			strcpy(nc_path, new);
			strcat(nc_path, "/");
			strcat(nc_path, child->name);
			inode* c_inode = dummyfs_inode_search(fs, oc_path);
			swisstable_map_erase(fs->inode_map, oc_path, strlen(oc_path));
			dummyfs_inode_add_explicit(fs, nc_path, c_inode);
			dummyfs_rename_subdirs(fs, child, oc_path, nc_path);
		}
	}
}

static int
dummyfs_rename (const char* old, const char* new, unsigned int flags) {
	(void)new;
	struct fuse_context* ctx = fuse_get_context();
	struct dummyfs* fs = (struct dummyfs*) ctx->private_data;
	int res = 0;

	inode* o_inode = dummyfs_inode_search(fs, old);
	inode* n_inode = dummyfs_inode_search(fs, new);

	if ((flags & RENAME_EXCHANGE ) == RENAME_EXCHANGE) {
		if (o_inode != NULL && n_inode != NULL) {
			struct fs_node* o_entry = dummyfs_entry_search(fs, o_inode);
			struct fs_node* n_entry = dummyfs_entry_search(fs, n_inode);
			struct fs_node swap = *o_entry;
			*o_entry = *n_entry;
			*n_entry = swap;
			return res;
		}
		res = -EINVAL;
		return res;
	}

	if ((flags & RENAME_NOREPLACE) == RENAME_NOREPLACE) {
		if (n_inode != NULL) {
			res = -EINVAL;
			return res;
		}
		struct fs_node* o_entry = dummyfs_entry_search(fs, o_inode);
		char op_path[strlen(old)];
		char od_name[strlen(old)];
		get_parent_path(old, op_path, od_name);

		char np_path[strlen(new)];
		char nd_name[strlen(new)];
		get_parent_path(new, np_path, nd_name);

		inode* op_inode = dummyfs_inode_search(fs, op_path);
		struct fs_node* op_entry = dummyfs_entry_search(fs, op_inode);

		inode* np_inode = dummyfs_inode_search(fs, np_path);
		struct fs_node* np_entry = dummyfs_entry_search(fs, np_inode);

		op_entry->children = g_list_remove(op_entry->children, o_entry);
		np_entry->children = g_list_append(np_entry->children, o_entry);
		char* name = malloc(strlen(nd_name) + 1);
		strcpy(name, nd_name);
		free(o_entry->name);
		o_entry->name = name;
		dummyfs_inode_add_explicit(fs, new, o_inode);
		swisstable_map_erase(fs->inode_map, old, strlen(old));
		dummyfs_rename_subdirs(fs, o_entry, old, new);
	}

	return 0;
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

	while (atomic_flag_test_and_set(&node->busy)) {}
	if (node != NULL && (node->stat.st_mode & S_IFDIR) == S_IFDIR) {
		filler(buf, ".", NULL, 0, 0);
		filler(buf, "..", NULL, 0, 0);
		GList* cur = node->children;
		while (cur!=NULL) {
			struct fs_node* child = (struct fs_node*) cur->data;
			if (child->stat.st_nlink > 0) {
				filler(buf,child->name, NULL, 0, 0);
			}
			cur = cur->next;
		}
	} else if (node == NULL) {
		res = -ENOENT;
	} else {
		// invalid type
		res = -EINVAL;
	}
	atomic_flag_clear(&node->busy);
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
		while (atomic_flag_test_and_set(&node->busy)) {}
		if ((long unsigned int) size > TEN_MIB) {
			res = -ENOSPC;
			return res;
		}
		off_t allocate_size = size;
		if ((long unsigned int) size < ONE_KB) {
			allocate_size = ONE_KB;
		}

		void* n_content = dummyfs_reallocate(fs, node->content, allocate_size, node->allocated_size);
		if (n_content == NULL) {
			res = -ENOMEM;
			return res;
		}
		node->content = n_content;
		node->allocated_size = allocate_size;
		node->stat.st_size = size;
		atomic_flag_clear(&node->busy);
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

	char parent_path[strlen(path)];
	char name[strlen(path)];
	get_parent_path(path, parent_path, name);

	inode* n_inode = dummyfs_inode_search(fs, path);
	if (n_inode == NULL) {
		res = -ENOENT;
		return res;
	}
	struct fs_node* node = dummyfs_entry_search(fs, n_inode);

	if (node != NULL) {
		while (atomic_flag_test_and_set(&node->busy)) {}
		node->stat.st_nlink = 0;
		atomic_flag_clear(&node->busy);
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
	// TODO: check inode
	struct fs_node* node = dummyfs_entry_search(fs, inode);
	if (node != NULL && node->stat.st_nlink > 0) {
		// Update fs tree
		while (atomic_flag_test_and_set(&node->busy)) {}
		if ((off_t)(offset + size) > node->stat.st_size) {
			size_t n_size = node->stat.st_size + size - (node->stat.st_size - offset);
			//printf("Increasing size to %ld\n", n_size);
			if (n_size > TEN_MIB) {
				res = -ENOSPC;
				return res;
			}
			if (n_size > node->allocated_size) {
				void* n_content = dummyfs_reallocate(fs, node->content, n_size, node->allocated_size);
				if (n_content == NULL) {
					res = -ENOMEM;
					return res;
				}
				node->content = n_content;
			}
			node->allocated_size = n_size;
			node->stat.st_size = n_size;
		}
		// Write to target memory
		memcpy(node->content + offset, buf, size);
		struct timespec ts;
		clock_gettime(CLOCK_REALTIME, &ts);
		node->stat.st_mtime = ts.tv_sec;
		res = size;
		atomic_flag_clear(&node->busy);
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
	.rename   = dummyfs_rename,
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

	printf("Initialized!\n");
	return fuse_main(argc, argv, &dummyfs_oper, &dfs);
}
