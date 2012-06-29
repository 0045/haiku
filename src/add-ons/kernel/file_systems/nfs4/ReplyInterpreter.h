/*
 * Copyright 2012 Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Paweł Dziepak, pdziepak@quarnos.org
 */
#ifndef REPLYINTERPRETER_H
#define REPLYINTERPRETER_H


#include <SupportDefs.h>

#include "NFS4Defs.h"
#include "RPCReply.h"


struct AttrValue {
	uint8	fAttribute;
	union {
			uint32		fValue32;
			uint64		fValue64;
			void*		fPointer;
	} fData;
};

struct DirEntry {
	const char*			fName;
	AttrValue*			fAttrs;
	uint32				fAttrCount;

						DirEntry();
						~DirEntry();
};

class ReplyInterpreter {
public:
						ReplyInterpreter(RPC::Reply* reply);
						~ReplyInterpreter();

			status_t	Access(uint32* supported, uint32* allowed);
			status_t	Close();
			status_t	GetAttr(AttrValue** attrs, uint32* count);
			status_t	GetFH(Filehandle* fh);
	inline	status_t	LookUp();
	inline	status_t	LookUpUp();
			status_t	Open(uint32* id, uint32* seq, bool* confirm);
			status_t	OpenConfirm(uint32* stateSeq);
	inline	status_t	PutFH();
	inline	status_t	PutRootFH();
			status_t	Read(void* buffer, uint32* size, bool* eof);
			status_t	ReadDir(uint64* cookie, DirEntry** dirents,
							uint32* count, bool* eof);
			status_t	SetClientID(uint64* clientid, uint64* verifier);
	inline	status_t	SetClientIDConfirm();

private:
			status_t	_DecodeAttrs(XDR::ReadStream& stream, AttrValue** attrs,
							uint32* count);
			status_t	_OperationError(Opcode op);

	static	status_t	_NFS4ErrorToHaiku(uint32 x);

			RPC::Reply*	fReply;
};


inline status_t
ReplyInterpreter::LookUp()
{
	return _OperationError(OpLookUp);
}


inline status_t
ReplyInterpreter::LookUpUp()
{
	return _OperationError(OpLookUpUp);
}


inline status_t
ReplyInterpreter::PutFH()
{
	return _OperationError(OpPutFH);
}


inline status_t
ReplyInterpreter::PutRootFH()
{
	return _OperationError(OpPutRootFH);
}


inline status_t
ReplyInterpreter::SetClientIDConfirm()
{
	return _OperationError(OpSetClientIDConfirm);
}


#endif	// REPLYINTERPRETER_H

