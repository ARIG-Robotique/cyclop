#pragma once

//File that provides most includes for both CDFR scenarios

#include <fstream>
#include <iostream>
#include <opencv2/core.hpp>


#include "GlobalConf.hpp"

#include "Cameras/VideoCaptureCamera.hpp"
#include "Cameras/CameraManager.hpp"
#include "ArucoPipeline/TrackerCube.hpp"
#include "ArucoPipeline/TopTracker.hpp"
#include "ArucoPipeline/StaticObject.hpp"
#include "ArucoPipeline/ObjectTracker.hpp"

#include "data/FrameCounter.hpp"
#include "Cameras/CameraView.hpp"

#include "data/senders/DataSender.hpp"


using namespace cv;
using namespace std;

//Camera steps pipeline
void BufferedPipeline(vector<Camera*> Cameras, 
	aruco::ArucoDetector& Detector, ObjectTracker* registry);

map<PacketType, bool> GetDefaultAllowMap();
