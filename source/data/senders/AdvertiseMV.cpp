#include "data/senders/AdvertiseMV.hpp"
#include <string>
#include <iostream>
using namespace std;

void AdvertiseMV::ThreadStartPoint(GenericTransport::NetworkInterface interface)
{
	string message = "Hello, this is a MV server";
	UDPTransport transport(50666, interface);
	cout << "Opening advertising server on " << interface.name << endl;
	while (!killmutex)
	{
		transport.Send(message.c_str(), message.length()+1, GenericTransport::BroadcastClient);
		this_thread::sleep_for(chrono::milliseconds(100));
	}
}

AdvertiseMV::AdvertiseMV(/* args */)
{
	auto interfaces = GenericTransport::GetInterfaces();
	cout << "Listing interfaces :" << endl;
	int chosen = -1;
	for (int i=0; i<interfaces.size(); i++)
	{
		auto& ni = interfaces[i];
		cout << ni.name << " @ " << ni.address << " & " << ni.mask << " * " << ni.broadcast << endl;
		if (ni.name != "lo")
		{
			thread* sendthread = new thread(&AdvertiseMV::ThreadStartPoint, this, ni);
			threads.push_back(sendthread);
		}
	}
}

AdvertiseMV::~AdvertiseMV()
{
	killmutex = true;
	for (auto &&sendthread : threads)
	{
		sendthread->join();
	}
}
