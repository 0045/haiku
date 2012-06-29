/*
 * Copyright 2012 Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Paweł Dziepak, pdziepak@quarnos.org
 */
#ifndef REQUESTBUILDER_H
#define REQUESTBUILDER_H


#include <SupportDefs.h>

#include "NFS4Defs.h"
#include "RPCCall.h"
#include "RPCServer.h"
#include "XDR.h"


class RequestBuilder {
public:
									RequestBuilder(Procedure p = ProcCompound);
									~RequestBuilder();

	inline	void					Reset(Procedure proc = ProcCompound);

			status_t				Access();
			status_t				Close(uint32 seq, const uint32* id,
										uint32 stateSeq);
			status_t				GetAttr(Attribute* attrs, uint32 count);
			status_t				GetFH();
			status_t				LookUp(const char* name);
			status_t				LookUpUp();
			status_t				Open(OpenClaim claim, uint32 seq,
										uint32 access, uint64 id, OpenCreate oc,
										uint32 ownerTime, uint32 ownerTID,
										const char* name);
			status_t				OpenConfirm(uint32 seq, const uint32* id,
										uint32 stateSeq);
			status_t				PutFH(const Filehandle& fh);
			status_t				PutRootFH();
			status_t				Read(const uint32* id, uint32 stateSeq,
										uint64 pos, uint32 len);
			status_t				ReadDir(uint32 count, uint64* cookie, 
										Attribute* attrs, uint32 attrCount);
			status_t				ReadLink();
			status_t				Renew(uint64 clientId);
			status_t				SetClientID(const RPC::Server* serv);
			status_t				SetClientIDConfirm(uint64 id, uint64 ver);

			RPC::Call*				Request();

private:
			void					_InitHeader();

			void					_AttrBitmap(XDR::WriteStream& stream,
										Attribute* attrs, uint32 count);

			uint32					fOpCount;
			XDR::Stream::Position	fOpCountPosition;

			Procedure				fProcedure;

			RPC::Call*				fRequest;
};


inline void
RequestBuilder::Reset(Procedure proc)
{
	fRequest->Stream().Clear();
	fOpCount = 0;
	fProcedure = proc;
	delete fRequest;

	_InitHeader();
}


#endif	// REQUESTBUILDER_H

