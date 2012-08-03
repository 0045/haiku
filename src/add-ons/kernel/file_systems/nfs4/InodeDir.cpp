/*
 * Copyright 2012 Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Paweł Dziepak, pdziepak@quarnos.org
 */


#include "Inode.h"

#include <dirent.h>
#include <string.h>

#include "IdMap.h"
#include "Request.h"
#include "RootInode.h"


status_t
Inode::CreateDir(const char* name, int mode)
{
	return CreateObject(name, NULL, mode, NF4DIR);
}


status_t
Inode::OpenDir(OpenDirCookie* cookie)
{
	if (fType != NF4DIR)
		return B_NOT_A_DIRECTORY;

	status_t result = Access(R_OK);
	if (result != B_OK)
		return result;

	cookie->fFileSystem = fFileSystem;
	cookie->fSpecial = 0;
	cookie->fSnapshot = NULL;
	cookie->fCurrent = NULL;
	cookie->fEOF = false;

	return B_OK;
}


status_t
Inode::FillDirEntry(struct dirent* de, ino_t id, const char* name, uint32 pos,
	uint32 size)
{
	uint32 nameSize = strlen(name);
	const uint32 entSize = sizeof(struct dirent);

	if (pos + entSize + nameSize > size)
		return B_BUFFER_OVERFLOW;

	de->d_dev = fFileSystem->DevId();
	de->d_ino = id;
	de->d_reclen = entSize + nameSize;
	if (de->d_reclen % 8 != 0)
		de->d_reclen += 8 - de->d_reclen % 8;

	strcpy(de->d_name, name);

	return B_OK;
}


status_t
Inode::ReadDirUp(struct dirent* de, uint32 pos, uint32 size)
{
	do {
		RPC::Server* serv = fFileSystem->Server();
		Request request(serv);
		RequestBuilder& req = request.Builder();

		req.PutFH(fInfo.fHandle);
		req.LookUpUp();
		req.GetFH();

		if (fFileSystem->IsAttrSupported(FATTR4_FILEID)) {
			Attribute attr[] = { FATTR4_FILEID };
			req.GetAttr(attr, sizeof(attr) / sizeof(Attribute));
		}

		status_t result = request.Send();
		if (result != B_OK)
			return result;

		ReplyInterpreter& reply = request.Reply();

		if (HandleErrors(reply.NFS4Error(), serv))
			continue;

		reply.PutFH();
		result = reply.LookUpUp();
		if (result != B_OK)
			return result;

		FileHandle fh;
		reply.GetFH(&fh);

		uint64 fileId;
		if (fFileSystem->IsAttrSupported(FATTR4_FILEID)) {
			AttrValue* values;
			uint32 count;
			reply.GetAttr(&values, &count);
			if (result != B_OK)
				return result;

			fileId = values[0].fData.fValue64;
			delete[] values;
		} else
			fileId = fFileSystem->AllocFileId();

		return FillDirEntry(de, FileIdToInoT(fileId), "..", pos, size);
	} while (true);
}


status_t
Inode::GetDirSnapshot(DirectoryCacheSnapshot** _snapshot,
	OpenDirCookie* cookie, uint64* _change)
{
	DirectoryCacheSnapshot* snapshot = new DirectoryCacheSnapshot;
	if (snapshot == NULL)
		return B_NO_MEMORY;

	uint64 change = 0;
	uint64 dirCookie = 0;
	uint64 dirCookieVerf = 0;
	bool eof = false;

	while (!eof) {
		uint32 count;
		DirEntry* dirents;

		status_t result = ReadDirOnce(&dirents, &count, cookie, &eof, &change,
			&dirCookie, &dirCookieVerf);
		if (result != B_OK) {
			delete snapshot;
			return result;
		}

		uint32 i;
		for (i = 0; i < count; i++) {

			// FATTR4_FSID is mandatory
			void* data = dirents[i].fAttrs[0].fData.fPointer;
			FileSystemId* fsid = reinterpret_cast<FileSystemId*>(data);
			if (*fsid != fFileSystem->FsId())
				continue;

			ino_t id;
			if (dirents[i].fAttrCount == 2)
				id = FileIdToInoT(dirents[i].fAttrs[1].fData.fValue64);
			else
				id = FileIdToInoT(fFileSystem->AllocFileId());
	
			NameCacheEntry* entry = new NameCacheEntry(dirents[i].fName, id);
			if (entry == NULL || entry->fName == NULL) {
				if (entry->fName == NULL)
					delete entry;
				delete snapshot;
				delete[] dirents;
				return B_NO_MEMORY;
			}
			snapshot->fEntries.Add(entry);
		}

		delete[] dirents;
	}

	*_snapshot = snapshot;
	*_change = change;

	return B_OK;
}


status_t
Inode::ReadDir(void* _buffer, uint32 size, uint32* _count,
	OpenDirCookie* cookie)
{
	if (cookie->fEOF) {
		*_count = 0;
		return B_OK;
	}

	status_t result;
	if (cookie->fSnapshot == NULL) {
		fFileSystem->Revalidator().Lock();
		if (fCache->Lock() != B_OK) {
			fCache->ResetAndLock();
		} else {
			fFileSystem->Revalidator().RemoveDirectory(fCache);
		}

		cookie->fSnapshot = fCache->GetSnapshot();
		if (cookie->fSnapshot == NULL) {
			uint64 change;
			result = GetDirSnapshot(&cookie->fSnapshot, cookie, &change);
			if (result != B_OK) {
				fCache->Unlock();
				fFileSystem->Revalidator().Unlock();
				return result;
			}
			fCache->ValidateChangeInfo(change);
			fCache->SetSnapshot(cookie->fSnapshot);
		}
		cookie->fSnapshot->AcquireReference();
		fFileSystem->Revalidator().AddDirectory(fCache);
		fCache->Unlock();
		fFileSystem->Revalidator().Unlock();
	}

	char* buffer = reinterpret_cast<char*>(_buffer);
	uint32 pos = 0;
	uint32 i = 0;
	bool overflow = false;

	if (cookie->fSpecial == 0 && i < *_count) {
		struct dirent* de = reinterpret_cast<dirent*>(buffer + pos);

		status_t result;
		result = FillDirEntry(de, fInfo.fFileId, ".", pos, size);

		if (result == B_BUFFER_OVERFLOW)
			overflow = true;
		else if (result == B_OK) {
			pos += de->d_reclen;
			i++;
			cookie->fSpecial++;
		} else
			return result;
	}

	if (cookie->fSpecial == 1 && i < *_count) {
		struct dirent* de = reinterpret_cast<dirent*>(buffer + pos);
		
		status_t result;
		if (strcmp(fInfo.fName, "/"))
			result = ReadDirUp(de, pos, size);
		else {
			result = FillDirEntry(de, FileIdToInoT(fInfo.fFileId), "..", pos,
				size);
		}

		if (result == B_BUFFER_OVERFLOW)
			overflow = true;
		else if (result == B_OK) {
			pos += de->d_reclen;
			i++;
			cookie->fSpecial++;
		} else
			return result;
	}

	MutexLocker _(cookie->fSnapshot->fLock);
	for (; !overflow && i < *_count; i++) {
		struct dirent* de = reinterpret_cast<dirent*>(buffer + pos);

		if (cookie->fCurrent == NULL)
			cookie->fCurrent = cookie->fSnapshot->fEntries.Head();
		else {
			cookie->fCurrent
				= cookie->fSnapshot->fEntries.GetNext(cookie->fCurrent);
		}

		if (cookie->fCurrent == NULL) {
			cookie->fEOF = true;
			break;
		}

		if (FillDirEntry(de, cookie->fCurrent->fNode, cookie->fCurrent->fName,
			pos, size) == B_BUFFER_OVERFLOW) {
			overflow = true;
			break;
		}

		pos += de->d_reclen;
	}

	if (i == 0 && overflow)
		return B_BUFFER_OVERFLOW;

	*_count = i;

	return B_OK;
}

