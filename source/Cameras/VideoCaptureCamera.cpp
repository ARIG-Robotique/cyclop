#include "Cameras/VideoCaptureCamera.hpp"


#include <iostream> // for standard I/O
#include <iomanip>  // for controlling float print precision
#include <sstream>  // string to number conversion
#include <stdlib.h>
#include <opencv2/imgproc.hpp>


#include <opencv2/calib3d.hpp>

#include "thirdparty/list-devices.hpp"
#include "thirdparty/serialib.h"

#include "Cameras/Calibfile.hpp"
#include "data/FrameCounter.hpp"
#include "Cameras/CameraView.hpp"

#include "ArucoPipeline/TrackedObject.hpp" //CameraView
#include "ArucoPipeline/ObjectTracker.hpp"
#include "GlobalConf.hpp"

using namespace cv;
using namespace std;

bool VideoCaptureCamera::StartFeed()
{
	auto globalconf = GetCaptureConfig();
	if (connected)
	{
		return false;
	}
	FrameBuffer = BufferedFrame();

	string pathtodevice = Settings.DeviceInfo.device_paths[0];	
	
	if (Settings.StartType == CameraStartType::CUDA)
	{
		Settings.StartPath = pathtodevice;
		Settings.ApiID = CAP_ANY;

		//d_reader = cudacodec::createVideoReader(Settings.DeviceInfo.device_paths[0], {
		//	CAP_PROP_FRAME_WIDTH, Settings.Resolution.width, 
		//	CAP_PROP_FRAME_HEIGHT, Settings.Resolution.height, 
		//	CAP_PROP_FPS, Settings.Framerate/Settings.FramerateDivider,
		//	CAP_PROP_BUFFERSIZE, 1});

	}
	else
	{
		ostringstream sizestream;
		sizestream << "width=(int)" << Settings.Resolution.width
				<< ", height=(int)" << Settings.Resolution.height;
		ostringstream sizestreamnocrop;
		sizestreamnocrop << "width=(int)" << Settings.Resolution.width + globalconf.CropRegion.x + globalconf.CropRegion.width
				<< ", height=(int)" << Settings.Resolution.height + globalconf.CropRegion.y + globalconf.CropRegion.height;
		switch (Settings.StartType)
		{
		case CameraStartType::GSTREAMER_NVARGUS:
			{
				//Settings.StartPath = "nvarguscamerasrc ! nvvidconv ! videoconvert ! appsink drop=1";
				ostringstream capnamestream;
				int sensorid = Settings.DeviceInfo.bus_info.back() == '4' ? 1 : 0;
				
				capnamestream << "nvarguscamerasrc sensor-id=" << sensorid << " ! video/x-raw(memory:NVMM), " 
				<< sizestreamnocrop.str() << ", framerate=(fraction)"
				<< (int)Settings.Framerate << "/" << (int)Settings.FramerateDivider 
				<< " ! nvvidconv flip-method=2 left=" << globalconf.CropRegion.x << " top=" << globalconf.CropRegion.y 
				<< " right=" << Settings.Resolution.width + globalconf.CropRegion.x 
				<< " bottom=" << Settings.Resolution.height + globalconf.CropRegion.y
				<< " ! video/x-raw, format=(string)BGRx, " << sizestream.str() << " ! videoconvert ! video/x-raw, format=BGR ! appsink drop=1";
				Settings.StartPath = capnamestream.str();
				Settings.ApiID = CAP_GSTREAMER;
			}
			break;
		case CameraStartType::GSTREAMER_CPU:
		case CameraStartType::GSTREAMER_NVDEC:
		case CameraStartType::GSTREAMER_JETSON:
			{
				ostringstream capnamestream;
				capnamestream << "v4l2src device=" << pathtodevice << " io-mode=4 ! image/jpeg, width=" 
				<< Settings.Resolution.width << ", height=" << Settings.Resolution.height << ", framerate="
				<< (int)Settings.Framerate << "/" << (int)Settings.FramerateDivider << ", num-buffers=1 ! ";
				if (Settings.StartType == CameraStartType::GSTREAMER_CPU)
				{
					capnamestream << "jpegdec ! videoconvert";
				}
				else if (Settings.StartType == CameraStartType::GSTREAMER_JETSON)
				{
					capnamestream << "nvv4l2decoder mjpeg=1 ! nvvidconv ! video/x-raw,format=BGRx ! videoconvert";
				}
				else if(Settings.StartType == CameraStartType::CUDA)
				{
					capnamestream << "nvdec ! glcolorconvert ! gldownload";
				}
				capnamestream << " ! video/x-raw, format=BGR ! appsink drop=1";
				Settings.StartPath = capnamestream.str();
				Settings.ApiID = CAP_GSTREAMER;
			}
			break;
		default:
			cerr << "WARNING : Unrecognised Camera Start Type in VideoCaptureCamera, defaulting to auto API" << endl;
		case CameraStartType::ANY:
			Settings.StartPath = pathtodevice;
			Settings.ApiID = CAP_ANY;
			break;
		}
		char commandbuffer[1024];
		snprintf(commandbuffer, sizeof(commandbuffer), "v4l2-ctl -d %s -c exposure_auto=%d,exposure_absolute=%d", pathtodevice.c_str(), 1, 32);
		cout << "Aperture system command : " << commandbuffer << endl;
		system(commandbuffer);
		feed = new VideoCapture();
		cout << "Opening device at \"" << Settings.StartPath << "\" with API id " << Settings.ApiID << endl;
		feed->open(Settings.StartPath, Settings.ApiID);
		if (Settings.StartType == CameraStartType::ANY)
		{
			feed->set(CAP_PROP_FOURCC, VideoWriter::fourcc('M', 'J', 'P', 'G'));
			//feed->set(CAP_PROP_FOURCC, VideoWriter::fourcc('Y', 'U', 'Y', 'V'));
			feed->set(CAP_PROP_FRAME_WIDTH, Settings.Resolution.width);
			feed->set(CAP_PROP_FRAME_HEIGHT, Settings.Resolution.height);
			feed->set(CAP_PROP_FPS, Settings.Framerate/Settings.FramerateDivider);
			feed->set(CAP_PROP_AUTO_EXPOSURE, 1) ;
			//feed->set(CAP_PROP_EXPOSURE, 32) ;
			//feed->set(CAP_PROP_BUFFERSIZE, 1);
		}
		//cout << "Success opening ? " << feed->isOpened() << endl;
		/*if (!feed->isOpened())
		{
			exit(EXIT_FAILURE);
		}*/
		
	}
	
	connected = true;
	return true;
}

bool VideoCaptureCamera::Grab()
{
	if (!connected)
	{
		return false;
	}
	BufferedFrame& buff = FrameBuffer;
	bool grabsuccess = false;
	grabsuccess = feed->grab();
	if (grabsuccess)
	{
		buff.Status.HasGrabbed = true;
		buff.CaptureTick = getCPUTickCount();
		RegisterNoError();
	}
	else
	{
		cerr << "Failed to grab frame for camera " << Settings.DeviceInfo.device_description <<endl;
		buff.Status = BufferStatus();
		RegisterError();
	}
	
	return grabsuccess;
}

bool VideoCaptureCamera::Read()
{
	BufferedFrame& buff = FrameBuffer;
	if (!connected)
	{
		return false;
	}
	bool ReadSuccess = false;
	bool HadGrabbed = buff.Status.HasGrabbed;
	buff.Reset();
	if (HadGrabbed)
	{
		ReadSuccess = feed->retrieve(buff.FrameRaw.CPUFrame);
	}
	else
	{
		ReadSuccess = feed->read(buff.FrameRaw.CPUFrame);
		buff.CaptureTick = getCPUTickCount();
	}
	buff.FrameRaw.HasCPU = ReadSuccess;
	
	if (!ReadSuccess)
	{
		cerr << "Failed to read frame for camera " << Settings.DeviceInfo.device_description <<endl;
		RegisterError();
		return false;
	}
	RegisterNoError();
	return true;
}