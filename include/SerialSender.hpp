#pragma once

#include <opencv2/core.hpp>
#include <vector>
#include <fstream>
#include <iostream>
#include "thirdparty/serialib.h"
#include "data/SerialPacket.hpp"
#include "TrackedObjects/TrackedObject.hpp"

using namespace std;
using namespace cv;

class SerialSender
{
private:
	vector<TrackerCube*> Robots;
	serialib* Bridge;

	int64 StartTick;

public:
	SerialSender(serialib* InBridge);
	~SerialSender();

	void RegisterRobot(TrackerCube* tracker);

	void SendPacket();

	void PrintCSVHeader(ofstream &file);

	void PrintCSV(ofstream &file);

	static vector<String> autoDetectTTYUSB();

};
