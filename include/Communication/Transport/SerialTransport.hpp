#pragma once

#include <Communication/Transport/GenericTransport.hpp>
#include <thirdparty/serialib.h>

#include <vector>
#include <cstring>

//Serial transport layer with SLIP encoding

/*class SerialTransport : public GenericTransport
{
private:
	serialib* Bridge;
	bool Connected;
public:

	static std::vector<std::string> autoDetectTTYUSB();

	SerialTransport(unsigned int BaudRate, bool SelfDetectSerial);

	virtual void Broadcast(const void *buffer, int length) override;

	virtual int Receive(void *buffer, int maxlength, int client = -1, bool blocking=false) override;

	virtual bool Send(const void* buffer, int length, int client = -1) override;
};*/