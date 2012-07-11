/*
 * Copyright 2012 Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Paweł Dziepak, pdziepak@quarnos.org
 */


#include "Connection.h"

#include <arpa/inet.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <util/kernel_cpp.h>
#include <net/dns_resolver.h>


#define	LAST_FRAGMENT	0x80000000
#define MAX_PACKET_SIZE	65535


bool
ServerAddress::operator==(const ServerAddress& x)
{
	return fAddress == x.fAddress && fPort == x.fPort
			&& fProtocol == x.fProtocol;
}

bool
ServerAddress::operator<(const ServerAddress& x)
{
	return fAddress < x.fAddress ||
			(fAddress == x.fAddress && fPort < x.fPort) ||
			(fAddress == x.fAddress && fPort == x.fPort &&
				fProtocol < x.fProtocol);
}


ServerAddress&
ServerAddress::operator=(const ServerAddress& x)
{
	fAddress = x.fAddress;
	fPort = x.fPort;
	fProtocol = x.fProtocol;
	return *this;
}


status_t
ServerAddress::ResolveName(const char* name, ServerAddress* addr)
{
	addr->fPort = 2049;
	addr->fProtocol = IPPROTO_UDP;

	struct in_addr iaddr;
	if (inet_aton(name, &iaddr) != 0) {
		addr->fAddress = ntohl(iaddr.s_addr);
		return B_OK;
	}

	addrinfo* ai;
	status_t result = getaddrinfo(name, NULL, NULL, &ai);
	if (result != B_OK)
		return result;

	addrinfo* current = ai;
	while (current != NULL) {
		if (current->ai_family == AF_INET) {
			sockaddr_in* sin = reinterpret_cast<sockaddr_in*>(current->ai_addr);

			addr->fAddress = ntohl(sin->sin_addr.s_addr);

			freeaddrinfo(ai);
			return B_OK;
		}

		current = current->ai_next;
	}

	freeaddrinfo(ai);
	return B_NAME_NOT_FOUND;
}


Connection::Connection(const sockaddr_in& addr, int proto)
	:
	fWaitCancel(create_sem(0, NULL)),
	fSock(-1),
	fProtocol(proto),
	fServerAddress(addr)
{
	mutex_init(&fSockLock, NULL);
}


ConnectionStream::ConnectionStream(const sockaddr_in& addr, int proto)
	:
	Connection(addr, proto)
{
}


ConnectionPacket::ConnectionPacket(const sockaddr_in& addr, int proto)
	:
	Connection(addr, proto)
{
}


Connection::~Connection()
{
	if (fSock != -1)
		close(fSock);
	mutex_destroy(&fSockLock);
	delete_sem(fWaitCancel);
}


status_t
Connection::GetLocalID(ServerAddress* addr)
{
	struct sockaddr_in saddr;
	socklen_t slen = sizeof(addr);
	status_t result = getsockname(fSock, (struct sockaddr*)&saddr, &slen);
	if (result != B_OK)
		return result;

	addr->fProtocol = fProtocol;
	addr->fPort = ntohs(saddr.sin_port);
	addr->fAddress = ntohl(saddr.sin_addr.s_addr);

	return B_OK;
}


status_t
ConnectionStream::Send(const void* buffer, uint32 size)
{
	status_t result;

	uint32* buf = (uint32*)malloc(size + sizeof(uint32));
	if (buf == NULL)
		return B_NO_MEMORY;

	buf[0] = htonl(size | LAST_FRAGMENT);
	memcpy(buf + 1, buffer, size);

	// More than one threads may send data and ksend is allowed to send partial
	// data. Need a lock here.
	uint32 sent = 0;
	mutex_lock(&fSockLock);
	do {
		result = send(fSock, buf + sent, size + sizeof(uint32) - sent, 0);
		sent += result;
	} while (result > 0 && sent < size + sizeof(uint32));
	mutex_unlock(&fSockLock);
	if (result < 0) {
		result = errno;
		free(buf);
		return result;
	} else if (result == 0) {
		free(buf);
		return B_IO_ERROR;
	}

	free(buf);
	return B_OK;
}


status_t
ConnectionPacket::Send(const void* buffer, uint32 size)
{
	// send on DGRAM sockets is atomic. No need to lock.
	status_t result = send(fSock, buffer,  size, 0);
	if (result < 0)
		return errno;

	return B_OK;
}


status_t
ConnectionStream::Receive(void** pbuffer, uint32* psize)
{
	status_t result;

	int32 size = 0;
	void* buffer = NULL;

	int32 record_size;
	bool last_one;

	object_wait_info object[2];
	object[0].object = fWaitCancel;
	object[0].type = B_OBJECT_TYPE_SEMAPHORE;
	object[0].events = B_EVENT_ACQUIRE_SEMAPHORE;

	object[1].object = fSock;
	object[1].type = B_OBJECT_TYPE_FD;
	object[1].events = B_EVENT_READ;

	do {
		result = wait_for_objects(object, 2);
		if (result < B_OK ||
			(object[0].events & B_EVENT_ACQUIRE_SEMAPHORE) != 0) {
			free(buffer);
			return ECONNABORTED;
		} else if ((object[1].events & B_EVENT_READ) == 0)
			continue;

		// There is only one listener thread per connection. No need to lock.
		uint32 received = 0;
		do {
			result = recv(fSock, &record_size + received,
							sizeof(record_size) - received, 0);
			received += result;
		} while (result > 0 && received < sizeof(record_size));
		if (result < 0) {
			result = errno;
			free(buffer);
			return result;
		} else if (result == 0) {
			free(buffer);
			return ECONNABORTED;
		}

		record_size = ntohl(record_size);
		last_one = record_size < 0;
		record_size &= LAST_FRAGMENT - 1;

		void* ptr = realloc(buffer, size + record_size);
		if (ptr == NULL) {
			free(buffer);
			return B_NO_MEMORY;
		} else
			buffer = ptr;

		received = 0;
		do {
			result = recv(fSock, (uint8*)buffer + size + received,
							record_size - received, 0);
			received += result;
		} while (result > 0 && received < sizeof(record_size));
		if (result < 0) {
			result = errno;
			free(buffer);
			return result;
		}

		size += record_size;
	} while (!last_one);


	*pbuffer = buffer;
	*psize = size;

	return B_OK;
}


status_t
ConnectionPacket::Receive(void** pbuffer, uint32* psize)
{
	status_t result;
	int32 size = MAX_PACKET_SIZE;
	void* buffer = malloc(size);

	if (buffer == NULL)
		return B_NO_MEMORY;

	object_wait_info object[2];
	object[0].object = fWaitCancel;
	object[0].type = B_OBJECT_TYPE_SEMAPHORE;
	object[0].events = B_EVENT_ACQUIRE_SEMAPHORE;

	object[1].object = fSock;
	object[1].type = B_OBJECT_TYPE_FD;
	object[1].events = B_EVENT_READ;

	do {
		result = wait_for_objects(object, 2);
		if (result < B_OK ||
			(object[0].events & B_EVENT_ACQUIRE_SEMAPHORE) != 0) {
			free(buffer);
			return ECONNABORTED;
		} else if ((object[1].events & B_EVENT_READ) == 0)
			continue;
		break;
	} while (true);

	// There is only one listener thread per connection. No need to lock.
	size = recv(fSock, buffer, size, 0);
	if (size < 0) {
		result = errno;
		free(buffer);
		return result;
	} else if (size == 0) {
		free(buffer);
		return ECONNABORTED;
	}

	*pbuffer = buffer;
	*psize = size;

	return B_OK;
}


status_t
Connection::Connect(Connection **pconn, const ServerAddress& id)
{
	struct sockaddr_in addr;

	memset(&addr, 0, sizeof(addr));
	addr.sin_len = sizeof(struct sockaddr_in);
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(id.fAddress);
	addr.sin_port = htons(id.fPort);

	Connection* conn;
	switch (id.fProtocol) {
		case IPPROTO_TCP:
			conn = new(std::nothrow) ConnectionStream(addr, id.fProtocol);
			break;
		case IPPROTO_UDP:
			conn = new(std::nothrow) ConnectionPacket(addr, id.fProtocol);
			break;
		default:
			return B_BAD_VALUE;
	}
	if (conn == NULL)
		return B_NO_MEMORY;

	status_t result = conn->_Connect();
	if (result != B_OK) {
		delete conn;
		return result;
	}

	*pconn = conn;

	return B_OK;
}


status_t
Connection::_Connect()
{
	switch (fProtocol) {
		case IPPROTO_TCP:
			fSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
			break;
		case IPPROTO_UDP:
			fSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
			break;
		default:
			return B_BAD_VALUE;
	}
	if (fSock < 0)
		return errno;

	status_t result = connect(fSock, (struct sockaddr*)&fServerAddress,
								fServerAddress.sin_len);
	if (result < 0) {
		result = errno;
		close(fSock);
		return result;
	}

	return B_OK;
}


status_t
Connection::Reconnect()
{
	release_sem(fWaitCancel);
	close(fSock);
	acquire_sem(fWaitCancel);
	return _Connect();
}


void
Connection::Disconnect()
{
	release_sem(fWaitCancel);

	int sock = fSock;
	fSock = -1;
	close(sock);
}

