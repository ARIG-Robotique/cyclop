#pragma once

#include <opencv2/core.hpp>
#include <opencv2/cudacodec.hpp>

#include "thirdparty/list-devices.hpp"

using namespace cv;
using namespace std;

struct CameraView;

struct BufferStatus
{
    bool HasGrabbed;
	bool HasCaptured;
	bool HasUndistorted;
	bool HasResized;
	bool HasAruco;
	bool HasViews;

	BufferStatus()
	:HasGrabbed(false), HasCaptured(false), HasUndistorted(false), HasResized(false), HasAruco(false), HasViews(false)
	{}
};

enum class CameraStartType
{
	ANY,
	GSTREAMER_CPU,
	GSTREAMER_NVDEC,
	GSTREAMER_JETSON,
	GSTREAMER_NVARGUS,
	CUDA
};

struct CameraSettings
{
	//which api should be used ?
	CameraStartType StartType;

	//General data
	//Resolution of the frame to be captured
	Size Resolution;
	//Framerate
	uint8_t Framerate;
	//Framerate divider : you can set 60fps but only sample 1 of 2 frames to have less latency and less computation
	uint8_t FramerateDivider;
	//Size of the camera frame buffer, to pipeline opencv computations
	uint8_t BufferSize;
	//data from v4l2 about the device
	v4l2::devices::DEVICE_INFO DeviceInfo;

	//Initialisation string
	String StartPath;
	//API to be used for opening
	int ApiID;

	//FOV and center
	Mat CameraMatrix;
	//Distortion
	Mat distanceCoeffs;

	CameraSettings()
	:Resolution(-1,-1), Framerate(0), FramerateDivider(1),
	BufferSize(0), ApiID(-1)
	{}

	bool IsValid();
};

class MixedFrame
{
public:
	UMat CPUFrame;
	cuda::GpuMat GPUFrame;
	bool HasCPU;
	bool HasGPU;

public:

	MixedFrame()
	:HasCPU(false),
	HasGPU(false)
	{}

	MixedFrame(UMat& InCPUFrame)
	:CPUFrame(InCPUFrame),
	HasCPU(true),
	HasGPU(false)
	{}

	MixedFrame(cuda::GpuMat& InGPUFrame)
	:GPUFrame(InGPUFrame),
	HasGPU(true),
	HasCPU(false)
	{}

	bool IsValid();

	Size GetSize();

	bool GetCPUFrame(UMat& frame);

	bool GetGPUFrame(cuda::GpuMat& frame);

	bool MakeCPUAvailable();

	bool MakeGPUAvailable();


friend class Camera;
};

class ObjectTracker;

class BufferedFrame
{

public:
	MixedFrame FrameRaw;
	MixedFrame FrameUndistorted;
	BufferStatus Status;

	vector<MixedFrame> rescaledFrames;

	vector<int> markerIDs;
	vector<vector<Point2f>> markerCorners;
	vector<CameraView> markerViews;

public:

	BufferedFrame()
		:Status()
	{}

	bool GetFrameRaw(MixedFrame& OutFrame);

	bool GetFrameUndistorted(MixedFrame& OutFrame);

	bool GetRescaledFrame(int index, MixedFrame& OutFrame);

friend class Camera;
};