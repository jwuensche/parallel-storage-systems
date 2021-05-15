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

#define TWO_MB sizeof(char) * 2097152

typedef unsigned int inode;

struct dummyfs {
	// We use this later as we need to remove files again and reassign inodes again
	// For now this is not implemented
	// inode* inodes;
	inode cur_inode;
	struct fs_node* root;
	char** contents;
};

struct fs_node {
	const char* name;
	struct fs_node* children;
	size_t num_children;
	char is_directory;
	inode inode;
	struct stat stat;
};

void dummyfs_add_file(struct dummyfs* fs, const char* name, inode parent, struct fuse_file_info* fi, uid_t uid, gid_t gid) {
	// TODO: Add implementation to add file to the tree
	fs->cur_inode += 1;

	// Init new fsnode
	struct fs_node* c = &fs->root->children[fs->root->num_children];
	c->name = name;
	c->num_children = 0;
	c->is_directory = 0;
	c->inode = fs->cur_inode;

	// Init metadata
	struct stat* meta = &c->stat;
	meta->st_ino = fs->cur_inode;
	meta->st_uid = uid;
	meta->st_gid = gid;
	meta->st_size = 0;
	meta->st_blocks = 1;
	meta->st_blksize = TWO_MB;
	meta->st_dev = 1337;
	meta->st_mode = S_IFREG;
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	meta->st_atime = ts.tv_sec;
	meta->st_mtime = ts.tv_sec;
	meta->st_ctime = ts.tv_sec;
	meta->st_nlink = 1;

	fs->root->num_children += 1;
	// Init content and fs
	fs->contents[fs->cur_inode] = malloc(TWO_MB);
}

void dummy_add_directory(struct dummyfs* fs, const char* name, inode parent, struct fuse_file_info* fi) {
	// TODO: Add impl for directories
}

void dummyfs_init (struct dummyfs* fs) {
	// Current maximum of 2^12 entries in the fs, for testing enough...
	// fs->inodes = calloc(4096, sizeof(unsigned int));
	fs->cur_inode = 0;
	fs->contents = malloc(sizeof(char*) * 4096);

	fs->root = malloc(sizeof(struct fs_node));
	fs->root->name = "Belzebub";
	fs->root->children = malloc(sizeof(struct fs_node) * 256);
	fs->root->num_children = 0;
	fs->root->is_directory = 1;
	fs->root->inode = fs->cur_inode;

	// Add Dummy File for now
	dummyfs_add_file(fs, "matrix.out", fs->cur_inode, NULL, 1000, 1000);
}

/*
** Fetch the first part of the given string until / seperator is met
** If no separator is found the whole string is returned
*/
void get_first_element(const char* source, char* target) {
	for (size_t idx = 0; idx < strlen(source); idx += 1) {
		if (source[idx] == '/') {
			break;
		}
		target[idx] = source[idx];
	}
}

struct fs_node* dummyfs_find_node(struct dummyfs* fs, const char* path) {
	struct fs_node* target = NULL;
	for (size_t idx; idx < fs->root->num_children; idx += 1) {
		if (fs->root->children[idx].name) {

		}
	}
	return target;
}

static int
dummyfs_chmod (const char* path, mode_t mode, struct fuse_file_info* fi)
{
	return 0;
}

static int
dummyfs_chown (const char* path, uid_t uid, gid_t gid, struct fuse_file_info* fi)
{
	return 0;
}

static int
dummyfs_create (const char* path, mode_t mode, struct fuse_file_info* fi)
{
	return 0;
}

static int
dummyfs_getattr (const char* path, struct stat* stbuf, struct fuse_file_info* fi)
{
	printf("Entering getattr for path: %s\n", path);
	(void) fi;
	int res = 0;
	struct fuse_context* ctx = fuse_get_context();
	struct dummyfs* fs = (struct dummyfs*) ctx->private_data;

	memset(stbuf, 0, sizeof(struct stat));
	if (strcmp(path, "/") == 0) {
			stbuf->st_mode = S_IFDIR | 0755;
			stbuf->st_nlink = 2;
	} else if (strcmp(path+1, "matrix.out") == 0) {
		*stbuf = fs->root->children[0].stat;
	} else
			res = -ENOENT;
	return res;
}

static int
dummyfs_link (const char* from, const char* to)
{
	return 0;
}

static int
dummyfs_mkdir(const char* path, mode_t mode) {return 0;
}

static int
dummyfs_open (const char* path, struct fuse_file_info* fi)
{
	return 0;
}

static int
dummyfs_read (const char* path, char* buf, size_t size, off_t offset, struct fuse_file_info* fi)
{
	printf("Entering read for %s\n", path);
	struct fuse_context* ctx = fuse_get_context();
	struct dummyfs* fs = (struct dummyfs*) ctx->private_data;
	int res = 0;
	if (strcmp(path,"/") == 0) {
		printf("Write to root\n");
		res = -ENOENT;
	} else if (strcmp(path + 1,"matrix.out") == 0) {
		printf("READING from matrix.out\n");
		struct fs_node* node = &fs->root->children[0];
		inode ino = node->inode;
		// Write to target memory
		memcpy(buf, fs->contents[ino] + offset, size);
		// Update fs tree
		struct timespec ts;
		clock_gettime(CLOCK_REALTIME, &ts);
		node->stat.st_atime = ts.tv_sec;
		res = size;
	} else {
		printf("INVALID\n");
		res = -ENOENT;
	}

	return res;
}

static int
dummyfs_readdir (const char* path, void* buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* fi, enum fuse_readdir_flags flags)
{
	printf("Entering readdir\n");
	struct fuse_context* ctx = fuse_get_context();
	struct dummyfs* fs = (struct dummyfs*) ctx->private_data;

	filler(buf, ".", NULL, 0, 0);
	filler(buf, "..", NULL, 0, 0);

	// Act from root
	(void) path;

	printf("Filling root content \n");
	for (size_t idx = 0; idx < fs->root->num_children; idx += 1) {
		filler(buf, fs->root->children[idx].name, NULL, 0, 0);
	}
	printf("Done\n");

	return 0;
}

static int
dummyfs_rmdir (const char* path)
{
	return 0;
}

static int
dummyfs_statfs (const char* path, struct statvfs* stbuf)
{
	return 0;
}

static int
dummyfs_truncate (const char* path, off_t size, struct fuse_file_info* fi)
{
	return 0;
}

static int
dummyfs_unlink (const char* path)
{
	return 0;
}

static int
dummyfs_utimens (const char* path, const struct timespec ts[2], struct fuse_file_info* fi)
{
	return 0;
}

static int
dummyfs_write (const char* path, const char* buf, size_t size, off_t offset, struct fuse_file_info* fi)
{
	struct fuse_context* ctx = fuse_get_context();
	struct dummyfs* fs = (struct dummyfs*) ctx->private_data;
	int res = 0;
	if (strcmp(path,"/") == 0) {
		printf("Write to root\n");
		res = -ENOENT;
	} else if (strcmp(path + 1,"matrix.out") == 0) {
		printf("Writing to matrix.out\n");
		struct fs_node* node = &fs->root->children[0];
		inode ino = node->inode;
		// Write to target memory
		memcpy(fs->contents[ino] + offset, buf, size);
		printf("%s", fs->contents[ino]);
		// Update fs tree
		if (offset + size > node->stat.st_size) {
			node->stat.st_size += size - (node->stat.st_size - offset);
			printf("Increasing size to %d\n", node->stat.st_size);
		}
		struct timespec ts;
		clock_gettime(CLOCK_REALTIME, &ts);
		node->stat.st_mtime = ts.tv_sec;
		res = size;
	} else {
		printf("INVALID\n");
		res = -ENOENT;
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

	printf("Initialized!\n");
	return fuse_main(argc, argv, &dummyfs_oper, &dfs);
}
