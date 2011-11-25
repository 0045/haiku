/*
 * Copyright 2009-2011, Ingo Weinhold, ingo_weinhold@gmx.de.
 * Distributed under the terms of the MIT License.
 */


#include "UnpackingDirectory.h"

#include "DebugSupport.h"
#include "UnpackingAttributeCookie.h"
#include "UnpackingAttributeDirectoryCookie.h"
#include "Utils.h"


// #pragma mark - UnpackingDirectory


UnpackingDirectory::UnpackingDirectory(ino_t id)
	:
	Directory(id)
{
}


UnpackingDirectory::~UnpackingDirectory()
{
}


status_t
UnpackingDirectory::Init(Directory* parent, const char* name)
{
	return Directory::Init(parent, name);
}


mode_t
UnpackingDirectory::Mode() const
{
	if (PackageDirectory* packageDirectory = fPackageDirectories.Head())
		return packageDirectory->Mode();
	return S_IFDIR | S_IRUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
}


uid_t
UnpackingDirectory::UserID() const
{
	if (PackageDirectory* packageDirectory = fPackageDirectories.Head())
		return packageDirectory->UserID();
	return 0;
}


gid_t
UnpackingDirectory::GroupID() const
{
	if (PackageDirectory* packageDirectory = fPackageDirectories.Head())
		return packageDirectory->GroupID();
	return 0;
}


timespec
UnpackingDirectory::ModifiedTime() const
{
	if (PackageDirectory* packageDirectory = fPackageDirectories.Head())
		return packageDirectory->ModifiedTime();

	timespec time = { 0, 0 };
	return time;
}


off_t
UnpackingDirectory::FileSize() const
{
	return 0;
}


Node*
UnpackingDirectory::GetNode()
{
	return this;
}


status_t
UnpackingDirectory::AddPackageNode(PackageNode* packageNode)
{
	if (!S_ISDIR(packageNode->Mode()))
		return B_BAD_VALUE;

	PackageDirectory* packageDirectory
		= dynamic_cast<PackageDirectory*>(packageNode);

	PackageDirectory* other = fPackageDirectories.Head();
	bool isNewest = other == NULL
		|| packageDirectory->ModifiedTime() > other->ModifiedTime();

	if (isNewest)
		fPackageDirectories.Insert(other, packageDirectory);
	else
		fPackageDirectories.Add(packageDirectory);

	return B_OK;
}


void
UnpackingDirectory::RemovePackageNode(PackageNode* packageNode)
{
	bool isNewest = packageNode == fPackageDirectories.Head();
	fPackageDirectories.Remove(dynamic_cast<PackageDirectory*>(packageNode));

	// when removing the newest node, we need to find the next node (the list
	// is not sorted)
	PackageDirectory* newestNode = fPackageDirectories.Head();
	if (isNewest && newestNode != NULL) {
		PackageDirectoryList::Iterator it = fPackageDirectories.GetIterator();
		it.Next();
			// skip the first one
		while (PackageDirectory* otherNode = it.Next()) {
			if (otherNode->ModifiedTime() > newestNode->ModifiedTime())
				newestNode = otherNode;
		}

		fPackageDirectories.Remove(newestNode);
		fPackageDirectories.Insert(fPackageDirectories.Head(), newestNode);
	}
}


PackageNode*
UnpackingDirectory::GetPackageNode()
{
	return fPackageDirectories.Head();
}


status_t
UnpackingDirectory::Read(off_t offset, void* buffer, size_t* bufferSize)
{
	return B_IS_A_DIRECTORY;
}


status_t
UnpackingDirectory::Read(io_request* request)
{
	return B_IS_A_DIRECTORY;
}


status_t
UnpackingDirectory::ReadSymlink(void* buffer, size_t* bufferSize)
{
	return B_IS_A_DIRECTORY;
}


status_t
UnpackingDirectory::OpenAttributeDirectory(AttributeDirectoryCookie*& _cookie)
{
	return UnpackingAttributeDirectoryCookie::Open(fPackageDirectories.Head(),
		_cookie);
}


status_t
UnpackingDirectory::OpenAttribute(const char* name, int openMode,
	AttributeCookie*& _cookie)
{
	return UnpackingAttributeCookie::Open(fPackageDirectories.Head(), name,
		openMode, _cookie);
}


// #pragma mark - RootDirectory


RootDirectory::RootDirectory(ino_t id, const timespec& modifiedTime)
	:
	UnpackingDirectory(id),
	fModifiedTime(modifiedTime)
{
}


timespec
RootDirectory::ModifiedTime() const
{
	return fModifiedTime;
}
