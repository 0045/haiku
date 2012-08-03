/*
 * Copyright 2012 Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Paweł Dziepak, pdziepak@quarnos.org
 */
#ifndef INODE_H
#define INODE_H


#include "NFS4Inode.h"


class Inode : public NFS4Inode {
public:
	static			status_t	CreateInode(FileSystem* fs, const FileInfo& fi,
									Inode** inode);
								~Inode();

	inline			ino_t		ID() const;
	inline			mode_t		Type() const;
	inline			const char*	Name() const;
	inline			FileSystem*	GetFileSystem() const;

	inline			void*		FileCache();
					status_t	RevalidateFileCache();

	inline			OpenFileCookie*	WriteCookie();
	inline			void		SetWriteCookie(OpenFileCookie* cookie);

					status_t	LookUp(const char* name, ino_t* id);

					status_t	Access(int mode);

					status_t	Commit();

					status_t	CreateObject(const char* name, const char* path,
									int mode, FileType type);

					status_t	CreateLink(const char* name, const char* path,
									int mode);

					status_t	Link(Inode* dir, const char* name);
					status_t	Remove(const char* name, FileType type);
	static			status_t	Rename(Inode* from, Inode* to,
									const char* fromName, const char* toName);

					status_t	Stat(struct stat* st);
					status_t	WriteStat(const struct stat* st, uint32 mask);

					status_t	Create(const char* name, int mode, int perms,
									OpenFileCookie* cookie, ino_t* id);
					status_t	Open(int mode, OpenFileCookie* cookie);
					status_t	Close(OpenFileCookie* cookie);
					status_t	Read(OpenFileCookie* cookie, off_t pos,
									void* buffer, size_t* length, bool* eof);
					status_t	Write(OpenFileCookie* cookie, off_t pos,
									const void* buffer, size_t *_length);

					status_t	CreateDir(const char* name, int mode);
					status_t	OpenDir(OpenDirCookie* cookie);
					status_t	ReadDir(void* buffer, uint32 size,
									uint32* count, OpenDirCookie* cookie);

					status_t	TestLock(OpenFileCookie* cookie,
									struct flock* lock);
					status_t	AcquireLock(OpenFileCookie* cookie,
									const struct flock* lock, bool wait);
					status_t	ReleaseLock(OpenFileCookie* cookie,
									const struct flock* lock);
					status_t	ReleaseAllLocks(OpenFileCookie* cookie);

protected:
								Inode();

					status_t	ReadDirUp(struct dirent* de, uint32 pos,
									uint32 size);
					status_t	FillDirEntry(struct dirent* de, ino_t id,
									const char* name, uint32 pos, uint32 size);
					status_t	GetDirSnapshot(DirectoryCacheSnapshot**
									_snapshot, OpenDirCookie* cookie,
									uint64* _change);

					status_t	ChildAdded(const char* name, uint64 fileID,
									const FileHandle& fileHandle);

					status_t	UpdateAttrCache(bool force = false);

	static inline	status_t	CheckLockType(short ltype, uint32 mode);

	static inline	ino_t		FileIdToInoT(uint64 fileid);

					uint32		fType;

					struct stat	fAttrCache;
					mutex		fAttrCacheLock;
					time_t		fAttrCacheExpire;
	static const	time_t		kAttrCacheExpirationTime	= 60;

					DirectoryCache*	fCache;

					uint64		fChange;
					void*		fFileCache;
					OpenFileCookie*	fWriteCookie;
					mutex		fFileCacheLock;

					bool		fWriteDirty;
};


inline ino_t
Inode::FileIdToInoT(uint64 fileid)
{
	if (sizeof(ino_t) >= sizeof(uint64))
		return fileid;
	else
		return (ino_t)fileid ^ (fileid >>
					(sizeof(uint64) - sizeof(ino_t)) * 8);
}


inline ino_t
Inode::ID() const
{
	return FileIdToInoT(fInfo.fFileId);
}


inline mode_t
Inode::Type() const
{
	return sNFSFileTypeToHaiku[fType];
}


inline const char*
Inode::Name() const
{
	return fInfo.fName;
}


inline FileSystem*
Inode::GetFileSystem() const
{
	return fFileSystem;
}


inline void*
Inode::FileCache()
{
	return fFileCache;
}


inline OpenFileCookie*
Inode::WriteCookie()
{
	return fWriteCookie;
}


inline void
Inode::SetWriteCookie(OpenFileCookie* cookie)
{
	fWriteCookie = cookie;
}


#endif	// INODE_H

