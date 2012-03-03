/*
 * Copyright 2011, John Scipione, jscipione@gmail.com.
 * Distributed under the terms of the MIT License.
 */
#ifndef FS_DARWIN_H
#define FS_DARWIN_H

/* flags for the *at() functions */
#define AT_FDCWD		(-1)		/* CWD FD for the *at() functions */

#define AT_SYMLINK_NOFOLLOW	0x01	/* fstatat(), fchmodat(), fchownat(),
										utimensat() */
#define AT_SYMLINK_FOLLOW	0x02	/* linkat() */
#define AT_REMOVEDIR		0x04	/* unlinkat() */
#define AT_EACCESS			0x08	/* faccessat() */

int faccessat(int fd, const char* path, int accessMode, int flag);
int fchmodat(int fd, const char* path, mode_t mode, int flag);
int fchownat(int fd, const char* path, uid_t owner, gid_t group, int flag);
int fstatat(int fd, const char *path, struct stat *st, int flag);
int mkdirat(int fd, const char *path, mode_t mode);
int mkfifoat(int fd, const char *path, mode_t mode);
int mknodat(int fd, const char *name, mode_t mode, dev_t dev);
int renameat(int fromFD, const char* from, int toFD, const char* to);

ssize_t readlinkat(int fd, const char *path, char *buffer, size_t bufferSize);
int symlinkat(const char *toPath, int fd, const char *symlinkPath);
int unlinkat(int fd, const char *path, int flag);
int linkat(int toFD, const char *toPath, int linkFD, const char *linkPath, int flag);

#endif // FS_DARWIN_H
