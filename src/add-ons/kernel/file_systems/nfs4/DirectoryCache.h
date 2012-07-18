/*
 * Copyright 2012 Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Paweł Dziepak, pdziepak@quarnos.org
 */
#ifndef DIRECTORYCACHE_H
#define DIRECTORYCACHE_H


#include <lock.h>
#include <SupportDefs.h>
#include <util/DoublyLinkedList.h>
#include <util/SinglyLinkedList.h>


class Inode;

struct NameCacheEntry :
	public SinglyLinkedListLinkImpl<NameCacheEntry> {
			ino_t			fNode;
			const char*		fName;
};


class DirectoryCache : public DoublyLinkedListLinkImpl<DirectoryCache> {
public:
							DirectoryCache(Inode* inode);
							~DirectoryCache();

	inline	status_t		Lock();
	inline	void			Unlock();

			void			ResetAndLock();
			void			Trash();

			status_t		AddEntry(const char* name, ino_t node);
			void			RemoveEntry(const char* name);

	inline	SinglyLinkedList<NameCacheEntry>&	EntriesList();

			status_t		Revalidate();

	inline  status_t		ValidateChangeInfo(uint64 change);
	inline  void			SetChangeInfo(uint64 change);
	inline	uint64			ChangeInfo();

	inline	Inode*			GetInode();
	inline	time_t			ExpireTime();

	static	const bigtime_t	kExpirationTime		= 5000000;
private:
			SinglyLinkedList<NameCacheEntry>	fNameCache;

			Inode*			fInode;

			bool			fTrashed;
			mutex			fLock;

			uint64			fChange;
			bigtime_t		fExpireTime;
};


inline status_t
DirectoryCache::Lock()
{
	mutex_lock(&fLock);
	if (fTrashed) {
		mutex_unlock(&fLock);
		return B_ERROR;
	}

	return B_OK;
}

inline void
DirectoryCache::Unlock()
{
	mutex_unlock(&fLock);
}


inline	SinglyLinkedList<NameCacheEntry>&
DirectoryCache::EntriesList()
{
	return fNameCache;
}


inline status_t
DirectoryCache::ValidateChangeInfo(uint64 change)
{
	if (fTrashed || change != fChange) {
		Trash();
		change = fChange;
		fExpireTime = system_time() + kExpirationTime;
		return B_ERROR;
	}

	return B_OK;
}


inline void
DirectoryCache::SetChangeInfo(uint64 change)
{
	fExpireTime = system_time() + kExpirationTime;
	fChange = change;
}


inline uint64
DirectoryCache::ChangeInfo()
{
	return fChange;
}


inline Inode*
DirectoryCache::GetInode()
{
	return fInode;
}


inline time_t
DirectoryCache::ExpireTime()
{
	return fExpireTime;
}


#endif	// DIRECTORYCACHE_H
			
