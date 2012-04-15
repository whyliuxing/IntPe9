#include "Skeleton.h"
#include <Windows.h>

Skeleton *me;
void Skeleton::DbgPrint(const char* format, ...)
{
	//Reset the buffer
	memset(_dbgPrint, 0x0, 512);

	//Logic
	va_list args;
	va_start(args, format);
	vsprintf_s(_dbgPrint, 512, format, args);
	va_end(args);
	OutputDebugStringA(_dbg);
}

bool Skeleton::sendCommand()
{
	return false;
}

bool Skeleton::sendPacket(MessagePacket *packet)
{
	try
	{
		bool ret = _packetQue->try_send((char*)packet, packet->messagePacketSize(), 0); //instant returns, we can have packet loss but keeping client alive is more important
		return ret;
	}
	catch(...)
	{
		DbgPrint("Exception raised on try send, packetLength: %i, structLength: %i", packet->length, packet->messagePacketSize());
		return false;
	}
}

DWORD WINAPI commandListener(LPVOID lpParam) 
{
	CommandControll *command = (CommandControll*)new uint8[MQ_MAX_SIZE];
	uint32 recvdSize, priority;

	message_queue *queue = new message_queue(open_only, "CommandControll");
	while(me->isActive)
	{
		queue->receive(command, MQ_MAX_SIZE, recvdSize, priority);
		me->processCommand(command);
	}
	return 0;
} 

void skipUnused(uint8 *data, uint32 *c)
{
	uint32 i = *c;
	while(data[i] == ' ' || data[i] == '\r' || data[i] == '\n' || data[i] == '\0' || data[i] == '\t')
		i++;
	*c = i;
}
void Skeleton::processCommand(CommandControll *command)
{
	uint32 i = 0;
	uint8 *data = command->getData();
	while(i < command->length)
	{
		if(data[i] == '.')
		{
			i++;
			if(memcmp(&data[i], "settings", 8) == 0)
			{
				skipUnused(data, &i);
			}
		}
		i++;
	}

}

Skeleton::Skeleton()
{
	//Initializing of our DbgPrint buffer
	_dbg = (char*)malloc(521);
	memcpy(_dbg, "[IntPe9] ", 9);
	_dbgPrint = &_dbg[9];
	
	me = this;
	isActive = true;
	_upx = new Upx();
	CreateThread( NULL, 0, commandListener, NULL, 0, NULL);
	
	//Initialize all message_queue's
	_packetQue = new message_queue(open_only, "packetSniffer");
}

Skeleton::~Skeleton()
{
	me->isActive = false;
	free(_dbgPrint);
}