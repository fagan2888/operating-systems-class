/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

  Minor modifications and note by Andy Sayler (2012) <www.andysayler.com>

  Source: fuse-2.8.7.tar.gz examples directory
  http://sourceforge.net/projects/fuse/files/fuse-2.X/

  This program can be distributed under the terms of the GNU GPL.
  See the file COPYING.

  gcc -Wall `pkg-config fuse --cflags` fusexmp.c -o fusexmp `pkg-config fuse --libs`

  Note: This implementation is largely stateless and does not maintain
        open file handels between open and release calls (fi->fh).
        Instead, files are opened and closed as necessary inside read(), write(),
        etc calls. As such, the functions that rely on maintaining file handles are
        not implmented (fgetattr(), etc). Those seeking a more efficient and
        more complete implementation may wish to add fi->fh support to minimize
        open() and close() calls and support fh dependent functions.

*/

#define FUSE_USE_VERSION 28
#define HAVE_SETXATTR

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef linux
/* For pread()/pwrite() */
#define _XOPEN_SOURCE 500
#endif

#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <sys/time.h>
#ifdef HAVE_SETXATTR
#include <sys/xattr.h>
#endif

#include "aes-crypt.h"

#define KEY_STR ((context*) fuse_get_context()->private_data)->key_str
#define MIRROR_DIR ((context*) fuse_get_context()->private_data)->mirror_dir

#define DECRYPT 0
#define ENCRYPT 1
#define PASS_THROUGH -1

#define XATTR_NAME "user.pa5-encfs.encrypted"

typedef struct 
{
	char *key_str;
	char *mirror_dir;
} context;

// the following function is from http://stackoverflow.com/questions/8465006/how-to-concatenate-2-strings-in-c
char* concat(const char *s1, const char *s2)
{
    char *result = malloc(strlen(s1)+strlen(s2)+1);//+1 for the zero-terminator
    //in real code you would check for errors in malloc here
    strcpy(result, s1);
    strcat(result, s2);
    return result;
}

static int xmp_getattr(const char *path, struct stat *stbuf)
{
	int res;
	char *full_path = concat(MIRROR_DIR, path);

	res = lstat(full_path, stbuf);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_access(const char *path, int mask)
{
	int res;
	char *full_path = concat(MIRROR_DIR, path);

	res = access(full_path, mask);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_readlink(const char *path, char *buf, size_t size)
{
	int res;
	char *full_path = concat(MIRROR_DIR, path);

	res = readlink(full_path, buf, size - 1);
	if (res == -1)
		return -errno;

	buf[res] = '\0';
	return 0;
}


static int xmp_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
		       off_t offset, struct fuse_file_info *fi)
{
	DIR *dp;
	struct dirent *de;
	char *full_path = concat(MIRROR_DIR, path);

	(void) offset;
	(void) fi;

	dp = opendir(full_path);
	if (dp == NULL)
		return -errno;

	while ((de = readdir(dp)) != NULL) {
		struct stat st;
		memset(&st, 0, sizeof(st));
		st.st_ino = de->d_ino;
		st.st_mode = de->d_type << 12;
		if (filler(buf, de->d_name, &st, 0))
			break;
	}

	closedir(dp);
	return 0;
}

static int xmp_mknod(const char *path, mode_t mode, dev_t rdev)
{
	int res;
	char *full_path = concat(MIRROR_DIR, path);

	/* On Linux this could just be 'mknod(full_path, mode, rdev)' but this
	   is more portable */
	if (S_ISREG(mode)) {
		res = open(full_path, O_CREAT | O_EXCL | O_WRONLY, mode);
		if (res >= 0)
			res = close(res);
	} else if (S_ISFIFO(mode))
		res = mkfifo(full_path, mode);
	else
		res = mknod(full_path, mode, rdev);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_mkdir(const char *path, mode_t mode)
{
	int res;
	char *full_path = concat(MIRROR_DIR, path);

	res = mkdir(full_path, mode);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_unlink(const char *path)
{
	int res;
	char *full_path = concat(MIRROR_DIR, path);

	res = unlink(full_path);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_rmdir(const char *path)
{
	int res;
	char *full_path = concat(MIRROR_DIR, path);

	res = rmdir(full_path);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_symlink(const char *from, const char *to)
{
	int res;
	char *full_from = concat(MIRROR_DIR, from);
	char *full_to = concat(MIRROR_DIR, to);

	res = symlink(full_from, full_to);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_rename(const char *from, const char *to)
{
	int res;
	char *full_from = concat(MIRROR_DIR, from);
	char *full_to = concat(MIRROR_DIR, to);

	res = rename(full_from, full_to);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_link(const char *from, const char *to)
{
	int res;
	char *full_from = concat(MIRROR_DIR, from);
	char *full_to = concat(MIRROR_DIR, to);

	res = link(full_from, full_to);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_chmod(const char *path, mode_t mode)
{
	int res;
	char *full_path = concat(MIRROR_DIR, path);

	res = chmod(full_path, mode);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_chown(const char *path, uid_t uid, gid_t gid)
{
	int res;
	char *full_path = concat(MIRROR_DIR, path);

	res = lchown(full_path, uid, gid);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_truncate(const char *path, off_t size)
{
	int res;
	char *full_path = concat(MIRROR_DIR, path);

	res = truncate(full_path, size);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_utimens(const char *path, const struct timespec ts[2])
{
	int res;
	struct timeval tv[2];
	char *full_path = concat(MIRROR_DIR, path);

	tv[0].tv_sec = ts[0].tv_sec;
	tv[0].tv_usec = ts[0].tv_nsec / 1000;
	tv[1].tv_sec = ts[1].tv_sec;
	tv[1].tv_usec = ts[1].tv_nsec / 1000;

	res = utimes(full_path, tv);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_open(const char *path, struct fuse_file_info *fi)
{
	char *full_path = concat(MIRROR_DIR, path);

	int fd = open(full_path, fi->flags);
	if (fd == -1)
		return -errno;

	fi->fh = fd;

	free(full_path);
	return 0;
}

static int xmp_read(const char *path, char *buf, size_t size, off_t offset,
            struct fuse_file_info *fi)
{
    (void) fi;

    char *full_path = concat(MIRROR_DIR, path);
    char *decrypted_path = concat(full_path, ".decrypted");

    int operation = DECRYPT;

    // read xattr; perform a pass-through if file not encrypted
    char val[10];
    int ret = getxattr(full_path, XATTR_NAME, &val, sizeof(val));
    if (ret == -1 && errno == ENODATA)
    	operation = PASS_THROUGH;
    else if (ret == -1)
    	return -errno;

    if (strcmp(val, "false") == 0)
    	operation = PASS_THROUGH;

    // write decrypted file to temp file
    FILE *fp_encrypted = fopen(full_path, "rb");
    FILE *fp_decrypted = fopen(decrypted_path, "w+");
    if (!fp_encrypted || !fp_decrypted)
        return -errno;

    do_crypt(fp_encrypted, fp_decrypted, operation, KEY_STR);

    // read from decrypted file
    fseek(fp_decrypted, offset, SEEK_SET);
    unsigned int res = fread(buf, 1, size, fp_decrypted);
    if (res != size && !feof(fp_decrypted))
        res = -errno;

    // close file handles
    fclose(fp_encrypted);
    fclose(fp_decrypted);

    // delete temporary file
    remove(decrypted_path);

    // free memory
    free(full_path);
    free(decrypted_path);

    return res;
}

static int xmp_write(const char *path, const char *buf, size_t size,
             off_t offset, struct fuse_file_info *fi)
{
    (void) fi;

    char *full_path = concat(MIRROR_DIR, path);
    char *decrypted_path = concat(full_path, ".decrypted");

    int first_operation = DECRYPT;
    int second_operation = ENCRYPT;

    // read xattr; perform a pass-through if file not encrypted
    char val[10];
    int ret = getxattr(full_path, XATTR_NAME, val, sizeof(val));
    if (ret == -1 && errno == ENODATA) {
    	first_operation = PASS_THROUGH;
    	second_operation = PASS_THROUGH;
    }
    else if (ret == -1)
    	return -errno;

    if (strcmp(val, "false") == 0) {
    	first_operation = PASS_THROUGH;
    	second_operation = PASS_THROUGH;
    }

    // write decrypted file to temp file
    FILE *fp_encrypted = fopen(full_path, "rb+");
    FILE *fp_decrypted = fopen(decrypted_path, "w+");
    if (!fp_encrypted || !fp_decrypted)
        return -errno;

    do_crypt(fp_encrypted, fp_decrypted, first_operation, KEY_STR);

    // write modification of decrypted file
    fseek(fp_decrypted, offset, SEEK_SET);
    unsigned int res = fwrite(buf, 1, size, fp_decrypted);
    if (res != size && !feof(fp_decrypted))
        res = -errno;

    // write modified file back into original file
    fseek(fp_encrypted, 0, SEEK_SET);
    fseek(fp_decrypted, 0, SEEK_SET);
    do_crypt(fp_decrypted, fp_encrypted, second_operation, KEY_STR);

    // close file handles
    fclose(fp_encrypted);
    fclose(fp_decrypted);

    // delete temporary file
    remove(decrypted_path);

    // free memory
    free(full_path);
    free(decrypted_path);

    return res;
}

static int xmp_statfs(const char *path, struct statvfs *stbuf)
{
	char *full_path = concat(MIRROR_DIR, path);

	int res = statvfs(full_path, stbuf);
	if (res == -1)
		return -errno;

	free(full_path);
	return 0;
}

static int xmp_create(const char* path, mode_t mode, struct fuse_file_info* fi)
{
    (void) fi;

    int res;
    char *full_path = concat(MIRROR_DIR, path);

    res = creat(full_path, mode);
    if(res == -1)
		return -errno;

    close(res);

    // designate file as encrypted
    char *val = "true";
    int ret = setxattr(full_path, XATTR_NAME, val, sizeof(int), 0);
    if (ret == -1)
    	return -errno;

    free(full_path);
    return 0;
}

static int xmp_release(const char *path, struct fuse_file_info *fi)
{
	/* Just a stub.	 This method is optional and can safely be left
	   unimplemented */

	(void) path;
	(void) fi;
	return 0;
}

static int xmp_fsync(const char *path, int isdatasync,
		     struct fuse_file_info *fi)
{
	/* Just a stub.	 This method is optional and can safely be left
	   unimplemented */

	(void) path;
	(void) isdatasync;
	(void) fi;
	return 0;
}

#ifdef HAVE_SETXATTR
static int xmp_setxattr(const char *path, const char *name, const char *value,
			size_t size, int flags)
{
	char *full_path = concat(MIRROR_DIR, path);
	int res = lsetxattr(full_path, name, value, size, flags);
	if (res == -1)
		return -errno;
	return 0;
}

static int xmp_getxattr(const char *path, const char *name, char *value,
			size_t size)
{
	char *full_path = concat(MIRROR_DIR, path);
	int res = lgetxattr(full_path, name, value, size);
	if (res == -1)
		return -errno;
	return res;
}

static int xmp_listxattr(const char *path, char *list, size_t size)
{
	char *full_path = concat(MIRROR_DIR, path);
	int res = llistxattr(full_path, list, size);
	if (res == -1)
		return -errno;
	return res;
}

static int xmp_removexattr(const char *path, const char *name)
{
	char *full_path = concat(MIRROR_DIR, path);
	int res = lremovexattr(full_path, name);
	if (res == -1)
		return -errno;
	return 0;
}
#endif /* HAVE_SETXATTR */

static struct fuse_operations xmp_oper = {
	.getattr	= xmp_getattr,
	.access		= xmp_access,
	.readlink	= xmp_readlink,
	.readdir	= xmp_readdir,
	.mknod		= xmp_mknod,
	.mkdir		= xmp_mkdir,
	.symlink	= xmp_symlink,
	.unlink		= xmp_unlink,
	.rmdir		= xmp_rmdir,
	.rename		= xmp_rename,
	.link		= xmp_link,
	.chmod		= xmp_chmod,
	.chown		= xmp_chown,
	.truncate	= xmp_truncate,
	.utimens	= xmp_utimens,
	.open		= xmp_open,
	.read		= xmp_read,
	.write		= xmp_write,
	.statfs		= xmp_statfs,
	.create     = xmp_create,
	.release	= xmp_release, 
	.fsync		= xmp_fsync,
#ifdef HAVE_SETXATTR
	.setxattr	= xmp_setxattr,
	.getxattr	= xmp_getxattr,
	.listxattr	= xmp_listxattr,
	.removexattr	= xmp_removexattr,
#endif
};

int main(int argc, char *argv[])
{
	umask(0);

	// extract necessary arguments and then reconstruct argc and argv
	char *key_str = concat(argv[1], "");
	char *mirror_dir = concat(argv[2], "");
	argv[2] = argv[0];
	argc -= 2;

	// construct context struct from these arguments
	context *my_context = malloc(sizeof(context));
	my_context->key_str = key_str;
	my_context->mirror_dir = mirror_dir;

	return fuse_main(argc, argv + 2, &xmp_oper, my_context);
}
