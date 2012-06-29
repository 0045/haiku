/*
 * Copyright 2012 Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Paweł Dziepak, pdziepak@quarnos.org
 */
#ifndef FILESYSTEM_H
#define FILESYSTEM_H


#include <fs_info.h>

#include "InodeIdMap.h"
#include "NFS4Defs.h"
#include "NFS4Server.h"


class Inode;
class InodeIdMap;

class Filesystem {
public:
	static	status_t			Mount(Filesystem** pfs, RPC::Server* serv,
									const char* path, dev_t id);
								~Filesystem();

			status_t			GetInode(ino_t id, Inode** inode);
			Inode*				CreateRootInode();

			status_t			ReadInfo(struct fs_info* info);

			status_t			Migrate(const Filehandle& fh,
									const RPC::Server* serv);

			OpenFileCookie*		OpenFilesLock();
			void				OpenFilesUnlock();
	inline	uint32				OpenFilesCount();
			void				AddOpenFile(OpenFileCookie* cookie);
			void				RemoveOpenFile(OpenFileCookie* cookie);

	inline	bool				IsAttrSupported(Attribute attr) const;
	inline	uint32				ExpireType() const;

	inline	RPC::Server*		Server();
	inline	NFS4Server*			NFSServer();

	inline	const char*			Path() const;
	inline	const FilesystemId&	FsId() const;

	inline	uint64				AllocFileId();

	inline	dev_t				DevId() const;
	inline	InodeIdMap*			InoIdMap();

			Filesystem*			fNext;
			Filesystem*			fPrev;
private:
								Filesystem();

			OpenFileCookie*		fOpenFiles;
			uint32				fOpenCount;
			mutex				fOpenLock;

			uint32				fExpireType;
			uint32				fSupAttrs[2];

			FilesystemId		fFsId;
			const char*			fPath;
			mutex				fMigrationLock;

			const char*			fName;
			FileInfo			fRoot;
			Filehandle			fRootFH;

			RPC::Server*		fServer;

			vint64				fId;
			dev_t				fDevId;

			InodeIdMap			fInoIdMap;
};


inline uint32
Filesystem::OpenFilesCount()
{
	return fOpenCount;
}


inline bool
Filesystem::IsAttrSupported(Attribute attr) const
{
	return sIsAttrSet(attr, fSupAttrs, 2);
}


inline uint32
Filesystem::ExpireType() const
{
	return fExpireType;
}


inline RPC::Server*
Filesystem::Server()
{
	return fServer;
}


inline NFS4Server*
Filesystem::NFSServer()
{
	return reinterpret_cast<NFS4Server*>(fServer->PrivateData());
}


inline const char*
Filesystem::Path() const
{
	return fPath;
}


inline const FilesystemId&
Filesystem::FsId() const
{
	return fFsId;
}


inline uint64
Filesystem::AllocFileId()
{
	return atomic_add64(&fId, 1);
}


inline dev_t
Filesystem::DevId() const
{
	return fDevId;
}


inline InodeIdMap*
Filesystem::InoIdMap()
{
	return &fInoIdMap;
}


#endif	// FILESYSTEM_H

