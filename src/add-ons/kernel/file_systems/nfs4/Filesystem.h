/*
 * Copyright 2012 Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Paweł Dziepak, pdziepak@quarnos.org
 */
#ifndef FILESYSTEM_H
#define FILESYSTEM_H


#include "InodeIdMap.h"
#include "NFS4Defs.h"
#include "RPCServer.h"


class Inode;
class InodeIdMap;

class Filesystem {
public:
	static	status_t		Mount(Filesystem** pfs, RPC::Server* serv,
								const char* path, dev_t id);
							~Filesystem();

			status_t		GetInode(ino_t id, Inode** inode);
			Inode*			CreateRootInode();

	inline	uint32			FHExpiryType() const;
	inline	RPC::Server*	Server();
	inline	uint64			GetId();

	inline	dev_t			DevId() const;
	inline	InodeIdMap*		InoIdMap();
private:
							Filesystem();

			uint32			fFHExpiryType;

			Filehandle		fRootFH;

			RPC::Server*	fServer;

			vint64			fId;
			dev_t			fDevId;

			InodeIdMap		fInoIdMap;
};


inline RPC::Server*
Filesystem::Server()
{
	return fServer;
}


inline uint64
Filesystem::GetId()
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

