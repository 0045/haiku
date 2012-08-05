/*
 * Copyright 2012 Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Paweł Dziepak, pdziepak@quarnos.org
 */


#include "Inode.h"

#include <string.h>

#include <fs_cache.h>
#include <NodeMonitor.h>

#include "IdMap.h"
#include "Request.h"
#include "RootInode.h"


status_t
Inode::CreateState(const char* name, int mode, int perms, OpenState* state) {
	uint64 fileID;
	FileHandle handle;
	ChangeInfo changeInfo;

	status_t result = CreateFile(name, mode, perms, state, &changeInfo,
		&fileID, &handle);
	if (result != B_OK)
		return result;

	FileInfo fi;
	fi.fFileId = fileID;
	fi.fHandle = handle;
	fi.fParent = fInfo.fHandle;
	fi.fName = strdup(name);

	char* path = reinterpret_cast<char*>(malloc(strlen(name) + 2 +
		strlen(fInfo.fPath)));
	strcpy(path, fInfo.fPath);
	strcat(path, "/");
	strcat(path, name);
	fi.fPath = path;

	fFileSystem->InoIdMap()->AddEntry(fi, FileIdToInoT(fileID));

	if (fCache->Lock() == B_OK) {
		if (changeInfo.fAtomic
			&& fCache->ChangeInfo() == changeInfo.fBefore) {
			fCache->AddEntry(name, fileID, true);
			fCache->SetChangeInfo(changeInfo.fAfter);
		} else if (fCache->ChangeInfo() != changeInfo.fBefore)
			fCache->Trash();
		fCache->Unlock();
	}

	state->fFileSystem = fFileSystem;
	state->fInfo = fi;

	return B_OK;
}


status_t
Inode::Create(const char* name, int mode, int perms, OpenFileCookie* cookie,
	ino_t* id)
{
	cookie->fMode = mode;
	cookie->fLocks = NULL;

	MutexLocker _(fStateLock);

	OpenState* state = new OpenState;
	status_t result = CreateState(name, mode, perms, state);
	if (result != B_OK)
		return result;

	cookie->fOpenState = state;
	fOpenState = state;

	*id = FileIdToInoT(state->fInfo.fFileId);
	cookie->fOpenState = fOpenState;

	cookie->fFileSystem = fFileSystem;

	fFileSystem->AddOpenFile(cookie);
	fFileSystem->Root()->MakeInfoInvalid();

	return B_OK;
}


status_t
Inode::Open(int mode, OpenFileCookie* cookie)
{
	MutexLocker _(fStateLock);

	if (fOpenState == NULL) {
		OpenState* state = new OpenState;
		if (state == NULL)
			return B_NO_MEMORY;

		state->fInfo = fInfo;
		state->fFileSystem = fFileSystem;
		status_t result = OpenFile(state, mode, NULL);
		if (result != B_OK)
			return result;

		fOpenState = state;
	} else {
		int newMode = mode & O_RWMASK;
		int oldMode = fOpenState->fMode & O_RWMASK;
		if (oldMode != newMode && oldMode != O_RDWR) {
			status_t result = OpenFile(fOpenState, O_RDWR, NULL);
			if (result != B_OK)
				return result;
			fOpenState->fMode = O_RDWR;
		}
		fOpenState->AcquireReference();
	}

	cookie->fOpenState = fOpenState;

	cookie->fFileSystem = fFileSystem;
	cookie->fMode = mode;
	cookie->fLocks = NULL;

	fFileSystem->AddOpenFile(cookie);

	return B_OK;
}


status_t
Inode::Close(OpenFileCookie* cookie)
{
	fFileSystem->RemoveOpenFile(cookie);

	MutexLocker _(fStateLock);
	if (cookie->fOpenState != NULL)
		if (cookie->fOpenState->ReleaseReference() == 1)
			fOpenState = NULL;

	return B_OK;
}


status_t
Inode::Read(OpenFileCookie* cookie, off_t pos, void* buffer, size_t* _length,
	bool* eof)
{
	*eof = false;
	uint32 size = 0;

	status_t result;
	while (size < *_length && !*eof) {
		uint32 len = *_length - size;
		result = ReadFile(cookie, fOpenState, pos + size, &len,
			reinterpret_cast<char*>(buffer) + size, eof);
		if (result != B_OK) {
			if (size == 0)
				return result;
			else
				break;
		}

		size += len;
	}

	*_length = size;

	return B_OK;
}


status_t
Inode::Write(OpenFileCookie* cookie, off_t pos, const void* _buffer,
	size_t *_length)
{
	uint32 size = 0;
	const char* buffer = reinterpret_cast<const char*>(_buffer);

	fWriteDirty = true;

	while (size < *_length) {
		uint32 len = *_length - size;
		status_t result = WriteFile(cookie, fOpenState, pos + size, &len,
			buffer + size);
		if (result != B_OK) {
			if (size == 0)
				return result;
			else
				break;
		}

		size += len;
	}

	*_length = size;

	fMetaCache.GrowFile(size + pos);
	fFileSystem->Root()->MakeInfoInvalid();

	return B_OK;
}


status_t
Inode::Commit()
{
	if (!fWriteDirty)
		return B_OK;
	status_t result = CommitWrites();
	if (result != B_OK)
		return result;
	fWriteDirty = false;
	return B_OK;
}

