/*
IntPe9 a open source multi game, general, all purpose and educational packet editor
Copyright (C) 2012  Intline9 <Intline9@gmail.com>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "Winsock.h"

bool doFirst = true;
MessagePacket *sendBuf;
MessagePacket *recvBuf;

// CodeCave addresses
uint32 addressSend = 0;
uint32 addressRecv = 0;
uint32 addressWSARecv = 0;

Winsock::Winsock()
{

}

void Winsock::initialize()
{
	// Guard
	if(isGetInfo)
		return;

	// First create buffers!!! then hook
	sendBuf = (MessagePacket*)new uint8[MP_MAX_SIZE];
	recvBuf = (MessagePacket*)new uint8[MP_MAX_SIZE];
	
	// Hook WSA by IAT
	_oldWSASendTo = (defWSASendTo)_upx->hookIatFunction("ws2_32", "WSASendTo", (unsigned long)&newWSASendTo);
	_oldWSARecvFrom = (defWSARecvFrom)_upx->hookIatFunction("ws2_32", "WSARecvFrom", (unsigned long)&newWSARecvFrom);
	_oldWSASend = (defWSASend)_upx->hookIatFunction("ws2_32", "WSASend", (unsigned long)&newWSASend);
	_oldWSARecv = (defWSARecv)_upx->hookIatFunction("ws2_32", "WSARecv", (unsigned long)&newWSARecv);
	_oldSend = (defSend)_upx->hookIatFunction("ws2_32", "send", (unsigned long)&newSend);
	_oldRecv = (defRecv)_upx->hookIatFunction("ws2_32", "recv", (unsigned long)&newRecv);

	// Now check for every function if we succeeded and if not lets do some inline hooking
	try
	{
		if(!_oldSend)
		{
			addressSend = ((uint32)&send) + 5;
			DbgPrint("Hooking send by inline: %08X", send);
			if((uint32)&send)
				Memory::writeJump((uint8*)send, (uint8*)CaveSend);
		}
		if(!_oldRecv)
		{
			addressRecv = ((uint32)&recv) + 0x97;
			DbgPrint("Hooking recv by inline: %08X", addressRecv);
			if((uint32)&recv)
				Memory::writeJump((uint8*)addressRecv, (uint8*)CaveRecv, 1);
		}
		if(!_oldWSARecv)
		{
			addressWSARecv = ((uint32)&WSARecv) + 0x95;
			DbgPrint("Hooking WSARecv inline: %08X", addressWSARecv);
			if((uint32)&WSARecv)
				Memory::writeJump((uint8*)addressWSARecv, (uint8*)CaveWSARecv);
		}
	}
	catch(...)
	{
		DbgPrint("Something went terrible wrong with inline hooks");
	}

	start();
	DbgPrint("Winsock core started!");
}

void Winsock::finalize()
{
	// Guard
	if(isGetInfo)
		return;

	DbgPrint("Terminating winsock core");

	delete[]sendBuf;
	delete[]recvBuf;

	exit();
}

// CodeCave wrappers
__declspec(naked) void CaveSend()
{
	__asm
	{
		// Restore prolog
		mov edi, edi
		push ebp
		mov ebp, esp

		// Push stack and call our function
		pushad
		push [ebp+0x14]
		push [ebp+0x10]
		push [ebp+0xC]
		push [ebp+0x8]
		call newSend
		popad

		// Return with send
		jmp addressSend
	}
}

__declspec(naked) void CaveRecv()
{
	__asm
	{
		push eax // Eax contains ret bytes so save it
		push eax
		push [ebp+0x14]
		push [ebp+0x10]
		push [ebp+0x0C]
		push [ebp+0x08]
		call inlineRecv
		pop eax

		// Restore bytes 
		pop esi
		pop ebx
		leave 
		ret 16
	}
}

__declspec(naked) void CaveWSARecv()
{
	__asm
	{
		// Only call our hook function if there was no error
		cmp eax, 0
		jne finish

		push eax //Contains state
		push [ebp+0x20]
		push [ebp+0x1C]
		push [ebp+0x18]
		push [ebp+0x14]
		push [ebp+0x10]
		push [ebp+0x0C]
		push [ebp+0x08]
		call newWSARecv
		pop eax

		// Restore bytes
		finish:
		pop esi
		leave
		retn 1
	}
}

// HOOK wrappers
int WSAAPI newSend(SOCKET s, const char *buf, int len, int flags)
{
	// Get data and send it to front end
	sendBuf->reset();
	sendBuf->type = SEND;
	sendBuf->length = len;
	memcpy(sendBuf->getData(), buf, sendBuf->length);
	winsock->sendMessagePacket(sendBuf);
	
	if(addressSend) //Inline hook so let codecave jump back
		return 0;
	return winsock->_oldSend(s, buf, len, flags);
}

int WSAAPI newRecv(SOCKET s, char *buf, int len, int flags)
{
	int bytesRecved = winsock->_oldRecv(s, buf, len, flags);
	inlineRecv(s, buf, len, flags, bytesRecved);
	return bytesRecved;
}

void WSAAPI inlineRecv(SOCKET s, char *buf, int len, int flags, int bytesRecved)
{
	if(bytesRecved <= 0)
		return;

	// Get data and send it to front end
	sendBuf->reset();
	sendBuf->type = RECV;
	sendBuf->length = bytesRecved;
	memcpy(sendBuf->getData(), buf, sendBuf->length);
	winsock->sendMessagePacket(sendBuf);
}

int WSAAPI newWSASend(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount, LPDWORD lpNumberOfBytesSent, DWORD dwFlags, LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine)
{
	for(uint32 i = 0; i <= dwBufferCount; i++)
	{
		sendBuf->reset();
		sendBuf->type = WSASEND;
		sendBuf->length = lpBuffers[i].len;
		memcpy(sendBuf->getData(), lpBuffers[i].buf, sendBuf->length);
		winsock->sendMessagePacket(sendBuf);
	}

	return winsock->_oldWSASend(s, lpBuffers, dwBufferCount, lpNumberOfBytesSent, dwFlags, lpOverlapped, lpCompletionRoutine);
}

int WSAAPI newWSARecv(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount, LPDWORD lpNumberOfBytesRecvd, LPDWORD lpFlags, LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine)
{
	int errorNo = 0;
	
	if(!addressWSARecv)
		errorNo = winsock->_oldWSARecv(s, lpBuffers, dwBufferCount, lpNumberOfBytesRecvd, lpFlags, lpOverlapped, lpCompletionRoutine);

	if(MP_MAX_SIZE < *lpNumberOfBytesRecvd)
	{
		winsock->DbgPrint("Tried to sniff a packet that is crazy big: %i, and i only have space for: %i", *lpNumberOfBytesRecvd, MP_MAX_SIZE);
		return errorNo;
	}

	if(errorNo == 0 && *lpNumberOfBytesRecvd > 0)
	{
		sendBuf->reset();
		recvBuf->type = WSARECV;
		recvBuf->length = *lpNumberOfBytesRecvd;
		memcpy(recvBuf->getData(), lpBuffers[0].buf, recvBuf->length);
		winsock->sendMessagePacket(recvBuf);
	}

	return errorNo;
}

int WSAAPI newWSASendTo(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount, LPDWORD lpNumberOfBytesSent, DWORD dwFlags, const struct sockaddr *lpTo, int iToLen, LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine)
{
	for(uint32 i = 0; i <= dwBufferCount; i++)
	{
		sendBuf->reset();
		sendBuf->type = WSASENDTO;
		sendBuf->length = lpBuffers[i].len;
		memcpy(sendBuf->getData(), lpBuffers[i].buf, sendBuf->length);
		winsock->sendMessagePacket(sendBuf);
	}
	return winsock->_oldWSASendTo(s, lpBuffers, dwBufferCount, lpNumberOfBytesSent, dwFlags, lpTo, iToLen, lpOverlapped, lpCompletionRoutine);
}

int WSAAPI newWSARecvFrom(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount, LPDWORD lpNumberOfBytesRecvd, LPDWORD lpFlags, struct sockaddr *lpFrom, LPINT lpFromlen, LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine)
{
	int errorNo = winsock->_oldWSARecvFrom(s, lpBuffers, dwBufferCount, lpNumberOfBytesRecvd, lpFlags, lpFrom, lpFromlen, lpOverlapped, lpCompletionRoutine);

	if(MP_MAX_SIZE < *lpNumberOfBytesRecvd)
	{
		winsock->DbgPrint("Tried to sniff a packet that is crazy big: %i, and i only have space for: %i", *lpNumberOfBytesRecvd, MP_MAX_SIZE);
		return errorNo;
	}

	if(errorNo == 0 && *lpNumberOfBytesRecvd > 0)
	{
			sendBuf->reset();
			recvBuf->type = WSARECVFROM;
			recvBuf->length = *lpNumberOfBytesRecvd;
			memcpy(recvBuf->getData(), lpBuffers[0].buf, recvBuf->length);
			winsock->sendMessagePacket(recvBuf);
	}

	return errorNo;
}