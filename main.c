#include <nolibc.h>

#define _STR(X) #X
#define STR(X) _STR(X)
#define check(X) if((X) == -1) { perror(__FILE__ ":" STR(__LINE__) ": " #X); exit(1); }

int openat(int dirfd, const char *pathname, int flags, int mode)
{
	return syscall(__NR_openat, dirfd, pathname, flags, mode);
}

int mkdirat(int dirfd, const char *pathname, int mode)
{
	return syscall(__NR_mkdirat, dirfd, pathname, mode);
}

int fstatat(int dirfd, const char *pathname, struct stat *stat, int flags)
{
	return syscall(__NR_newfstatat, dirfd, pathname, stat, flags);
}

int unlinkat(int dirfd, const char *pathname, int flags)
{
	return syscall(__NR_unlinkat, dirfd, pathname, flags);
}

int readlinkat(int dirfd, const char *pathname, char *buf, size_t size)
{
	return syscall(__NR_readlinkat, dirfd, pathname, buf, size);
}

int symlinkat(const char *target, int dirfd, const char *linkpath)
{
	return syscall(__NR_symlinkat, target, dirfd, linkpath);
}

int fsopen(const char *fsname, int flags)
{
	return syscall(__NR_fsopen, fsname, flags);
}

int fsmount(int fd, int flags, int mount_attr)
{
	return syscall(__NR_fsmount, fd, flags, mount_attr);
}

int fsconfig(int fd, int cmd, const char *key, const void *value, int aux)
{
	return syscall(__NR_fsconfig, fd, cmd, key, value, aux);
}

int fchdir(int fd)
{
	return syscall(__NR_fchdir, fd);
}

int move_mount(int from_dirfd, const char *from_pathname, int to_dirfd, const char *to_pathname, int flags)
{
	return syscall(__NR_move_mount, from_dirfd, from_pathname, to_dirfd, to_pathname, flags);
}

void write_file(int fd, const char *buf, size_t size)
{
	ssize_t len;

	while (size) {
		len = write(fd, buf, size);
		check(len);

		buf += len;
		size -= len;
	}
}

void cp_file(int dst, int src)
{
	static char buf[0x100000];
	ssize_t len;

	do {
		len = read(src, buf, sizeof(buf));
		check(len);
		write_file(dst, buf, len);
	} while(len);
}

void mv_file(int dst_dir, int src_dir, const char *pathname, int mode)
{
	int src;
	int dst;

	src = openat(src_dir, pathname, O_RDONLY, 0);
	check(src);

	dst = openat(dst_dir, pathname, O_CREAT | O_WRONLY, mode);
	check(dst);

	cp_file(dst, src);
	close(src);
	close(dst);

	unlinkat(src_dir, pathname, 0);
}

void mv_link(int dst_dir, int src_dir, const char *pathname)
{
	char buf[PATH_MAX] = { 0 };
	ssize_t len;

	len = readlinkat(src_dir, pathname, buf, sizeof(buf));
	check(len);

	check(symlinkat(buf, dst_dir, pathname));
	check(unlinkat(src_dir, pathname, 0));
}

void mv_dir(int dst_dir, int src_dir, const char *pathname, int mode);

void mv_dirent(int dst_dir, int src_dir, struct linux_dirent64 *dirent)
{
	struct stat stat;

	printf("mv %s\n", dirent->d_name);

	check(fstatat(src_dir, dirent->d_name, &stat, AT_SYMLINK_NOFOLLOW));

	if (S_ISREG(stat.st_mode)) mv_file(dst_dir, src_dir, dirent->d_name, stat.st_mode & 07777);
	else if (S_ISLNK(stat.st_mode)) mv_link(dst_dir, src_dir, dirent->d_name);
	else if (S_ISDIR(stat.st_mode)) mv_dir(dst_dir, src_dir, dirent->d_name, stat.st_mode & 07777);
}

void mv_dir(int dst_dir, int src_dir, const char *pathname, int mode)
{
	int dst;
	int src;
	char buf[sizeof(struct linux_dirent64) + PATH_MAX];
	ssize_t len;
	struct linux_dirent64 *dirent;

	if (pathname) {
		src = openat(src_dir, pathname, O_DIRECTORY | O_RDONLY, 0);
		check(src);

		check(mkdirat(dst_dir, pathname, mode));
		dst = openat(dst_dir, pathname, O_DIRECTORY | O_RDONLY, 0);
		check(dst);
	} else {
		dst = dst_dir;
		src = src_dir;
	}

	while (1) {
		len = getdents64(src, (struct linux_dirent64 *) buf, sizeof(buf));
		check(len);
		if (len == 0) break;

		for (size_t offset = 0; offset < (size_t) len; offset += dirent->d_reclen) {
			dirent = (struct linux_dirent64 *) (buf + offset);
			if (strcmp(dirent->d_name, ".") == 0 || strcmp(dirent->d_name, "..") == 0) continue;
			mv_dirent(dst, src, dirent);
		}
	}

	if (pathname) {
		close(dst);
		close(src);

		unlinkat(src_dir, pathname, AT_REMOVEDIR);
	}
}

int main()
{
	int src;
	int fs;
	int dir;

	printf("hello\n");

	src = openat(AT_FDCWD, ".", O_DIRECTORY | O_RDONLY, 0);
	check(src);

	fs = fsopen("tmpfs", 0);
	check(fs);
	check(fsconfig(fs, FSCONFIG_SET_STRING, "mode", "0755", 0));
	check(fsconfig(fs, FSCONFIG_CMD_CREATE, NULL, NULL, 0));
	
	dir = fsmount(fs, 0, 0);
	check(dir);
	close(fs);

	mv_dir(dir, src, NULL, 0);
	close(src);


	check(fchdir(dir));
	check(move_mount(dir, "", AT_FDCWD, "/", MOVE_MOUNT_F_EMPTY_PATH));
	check(chroot("."));

	close(dir);

	check(execve("/sbin/init", (char *[]) { "/sbin/init", NULL }, (char *[]) { NULL }));

	return 0;
}
