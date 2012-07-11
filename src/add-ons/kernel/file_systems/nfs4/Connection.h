/*
 * Copyright 2012 Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Paweł Dziepak, pdziepak@quarnos.org
 */
#ifndef CONNECTION_H
#define CONNECTION_H


#include <netinet/in.h>

#include <lock.h>
#include <SupportDefs.h>


struct ServerAddress {
			uint32				fAddress;
			uint16				fPort;
			int					fProtocol;

			bool				operator==(const ServerAddress& address);
			bool				operator<(const ServerAddress& address);

			ServerAddress&		operator=(const ServerAddress& address);

	static	status_t			ResolveName(const char* name,
									ServerAddress* address);
};

class Connection {
public:
	static	status_t			Connect(Connection **connection,
									const ServerAddress& address);
	virtual						~Connection();

	virtual	status_t			Send(const void* buffer, uint32 size) = 0;
	virtual	status_t			Receive(void** buffer, uint32* size) = 0;

			status_t			GetLocalAddress(ServerAddress* address);

			status_t			Reconnect();
			void				Disconnect();

protected:
								Connection(const sockaddr_in& address,
									int protocol);
			status_t			Connect();

			sem_id				fWaitCancel;
			int					fSocket;
			mutex				fSocketLock;

			const int			fProtocol;
			const sockaddr_in	fServerAddress;
};

class ConnectionStream : public Connection {
public:
								ConnectionStream(const sockaddr_in& address,
									int protocol);

	virtual	status_t			Send(const void* buffer, uint32 size);
	virtual	status_t			Receive(void** buffer, uint32* size);
};

class ConnectionPacket : public Connection {
public:
								ConnectionPacket(const sockaddr_in& address,
									int protocol);

	virtual	status_t			Send(const void* buffer, uint32 size);
	virtual	status_t			Receive(void** buffer, uint32* size);
};

#endif	// CONNECTION_H

