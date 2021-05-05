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
	return 0;
}

static int
dummyfs_link (const char* from, const char* to)
{
	return 0;
}

static int
dummyfs_mkdir(const char* path, mode_t mode)
{
	return 0;
}

static int
dummyfs_open (const char* path, struct fuse_file_info* fi)
{
	return 0;
}

static int
dummyfs_read (const char* path, char* buf, size_t size, off_t offset, struct fuse_file_info* fi)
{
	return 0;
}

static int
dummyfs_readdir (const char* path, void* buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* fi, enum fuse_readdir_flags flags)
{
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
	return 0;
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
	return fuse_main(argc, argv, &dummyfs_oper, NULL);
}
