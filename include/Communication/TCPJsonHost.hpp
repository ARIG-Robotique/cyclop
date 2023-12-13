#pragma once

#include <memory>
#include <set>
#include <thread>
#include <Communication/Transport/GenericTransport.hpp>

class TCPJsonHost
{
private:
	std::set<std::unique_ptr<std::thread>> ThreadHandles;
	int Port;
	bool killed;
public:
	class CDFRExternal* ExternalRunner = nullptr;
	TCPJsonHost(int InPort);
	~TCPJsonHost();

	bool IsKilled() const
	{
		return killed;
	}

private:
	void ThreadEntryPoint(GenericTransport::NetworkInterface interface);
};