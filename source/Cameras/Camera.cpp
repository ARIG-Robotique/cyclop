#include "Cameras/Camera.hpp"

#include <iostream> // for standard I/O
#include <math.h>

#include <opencv2/core.hpp>
#include <opencv2/calib3d.hpp>

#include <opencv2/imgproc.hpp>

#include "math2d.hpp"
#include "math3d.hpp"

#include "Cameras/Calibfile.hpp"

#include "ArucoPipeline/ObjectTracker.hpp"
#include "GlobalConf.hpp"

using namespace cv;
using namespace std;

void Camera::RegisterError()
{
	errors += 10;
}

void Camera::RegisterNoError()
{
	errors = std::max(0, errors -1);
}

const CameraSettings* Camera::GetCameraSettings()
{
	return Settings.get();
}

bool Camera::SetCameraSetting(std::shared_ptr<CameraSettings> InSettings)
{
	if (connected)
	{
		cerr << "WARNING: Tried to set camera settings, but camera is already started ! Settings have not been applied." << endl;
		return false;
	}
	Settings = InSettings;
	return true;
}

bool Camera::SetCalibrationSetting(Mat CameraMatrix, Mat DistanceCoefficients)
{
	Settings->CameraMatrix = CameraMatrix;
	Settings->distanceCoeffs = DistanceCoefficients;
	HasUndistortionMaps = false;
	return true;
}

void Camera::GetCameraSettingsAfterUndistortion(Mat& CameraMatrix, Mat& DistanceCoefficients)
{
	CameraMatrix = Settings->CameraMatrix;
	//DistanceCoefficients = Settings.distanceCoeffs; //FIXME
	DistanceCoefficients = Mat::zeros(4,1, CV_64F);
}

bool Camera::StartFeed()
{
	cerr << "ERROR : Tried to start feed on base class Camera !" << endl;
	return false;
}

bool Camera::Grab()
{
	cerr << "ERROR : Tried to grab on base class Camera !" << endl;
	return false;
}

bool Camera::Read()
{
	cerr << "ERROR : Tried to read on base class Camera !" << endl;
	return false;
}

void Camera::Undistort()
{
	
	if (!HasUndistortionMaps)
	{
		Size cammatsz = Settings->CameraMatrix.size();
		if (cammatsz.height != 3 || cammatsz.width != 3)
		{
			RegisterError();
			cerr << "Asking for undistortion but camera matrix is invalid ! Camera " << Name << endl;
			return;
		}
		//cout << "Creating undistort map using Camera Matrix " << endl << setcopy.CameraMatrix << endl 
		//<< " and Distance coeffs " << endl << setcopy.distanceCoeffs << endl;
		Mat map1, map2;

		initUndistortRectifyMap(Settings->CameraMatrix, Settings->distanceCoeffs, Mat::eye(3,3, CV_64F), 
		Settings->CameraMatrix, Settings->Resolution, CV_32FC1, map1, map2);
		map1.copyTo(UndistMap1);
		map2.copyTo(UndistMap2);
		HasUndistortionMaps = true;
	}
	try
	{
		remap(LastFrameDistorted, LastFrameUndistorted, UndistMap1, UndistMap2, INTER_LINEAR);
	}
	catch(const std::exception& e)
	{
		std::cerr << "Camera undistort failed : " << e.what() << '\n';
		return;
	}
}

void Camera::GetFrame(CameraImageData& frame, bool Distorted)
{
	frame.Distorted = Distorted;
	frame.CameraName = Name;
	if (Distorted)
	{
		frame.CameraMatrix = Settings->CameraMatrix;
		frame.DistanceCoefficients = Settings->distanceCoeffs;
		frame.Image = LastFrameDistorted;
	}
	else
	{
		GetCameraSettingsAfterUndistortion(frame.CameraMatrix, frame.DistanceCoefficients);
		frame.Image = LastFrameUndistorted;
	}
}

/*void Camera::GetFrameUndistorted(UMat& frame)
{
	BufferedFrame& buff = FrameBuffer;
	if (!buff.FrameUndistorted.MakeCPUAvailable())
	{
		return;
	}
	frame = buff.FrameUndistorted.CPUFrame;
}


void Camera::GetOutputFrame(UMat& OutFrame, Rect window)
{
	BufferedFrame& buff = FrameBuffer;
	MixedFrame& frametoUse = buff.FrameUndistorted;
	if (!frametoUse.IsValid())
	{
		return;
	}
	double fz = 1;
	Size basesize = frametoUse.GetSize();
	Size winsize = window.size();
	Point2f offset(window.x, window.y);
	if (winsize == basesize)
	{
		frametoUse.MakeCPUAvailable();
		if (frametoUse.CPUFrame.channels() != 3)
		{
			cvtColor(frametoUse.CPUFrame, OutFrame(window), COLOR_GRAY2BGR);
		}
		else
		{
			frametoUse.CPUFrame.copyTo(OutFrame);
		}
		
	}
	else
	{
		Rect roi = ScaleToFit(basesize, window);
		fz = (double)roi.width / basesize.width;
		offset = roi.tl();
		#ifdef WITH_CUDA
		if (frametoUse.HasGPU)
		{
			cuda::GpuMat resz;
			UMat dl;
			cuda::resize(frametoUse.GPUFrame, resz, roi.size(), fz, fz, INTER_AREA);
			resz.download(OutFrame(roi));
		}
		#else
		if(0){}
		#endif
		else
		{
			resize(frametoUse.CPUFrame, OutFrame(roi), roi.size(), fz, fz, INTER_AREA);
		}
	}
	if (buff.Status.HasAruco)
	{
		int nummarkers= buff.markerCorners.size();
		vector<vector<Point2f>> ArucoCornersResized;
		ArucoCornersResized.reserve(nummarkers);
		vector<vector<Point2f>> ArucoCornersReprojected;
		ArucoCornersReprojected.reserve(nummarkers);
		vector<int> ArucoIDsReprojected;
		ArucoIDsReprojected.reserve(nummarkers);
		
		for (int i = 0; i < buff.markerCorners.size(); i++)
		{
			vector<Point2f> marker, markerReprojected;
			bool IsMarkerReprojected = buff.reprojectedCorners[i].size() == 4;
			for (int j = 0; j < buff.markerCorners[i].size(); j++)
			{
				marker.push_back(buff.markerCorners[i][j]*fz + offset);
				if (IsMarkerReprojected)
				{
					markerReprojected.push_back(Point2f(buff.reprojectedCorners[i][j])*fz + offset);
				}
			}
			ArucoCornersResized.push_back(marker);
			if (IsMarkerReprojected)
			{
				ArucoIDsReprojected.push_back(buff.markerIDs[i]);
				ArucoCornersReprojected.push_back(markerReprojected);
			}
			
		}
		aruco::drawDetectedMarkers(OutFrame, ArucoCornersResized, buff.markerIDs);
		if (ArucoCornersReprojected.size() > 0)
		{
			aruco::drawDetectedMarkers(OutFrame, ArucoCornersReprojected, ArucoIDsReprojected, cv::Scalar(255,0,0));
		}
		
	}
}

vector<ObjectData> Camera::ToObjectData(int BaseNumeral)
{
	ObjectData robot;
	robot.identity.numeral = BaseNumeral;
	robot.identity.type = PacketType::Camera;
	robot.location = Location;
	return {robot};
}

Affine3d Camera::GetObjectTransform(const CameraArucoData& CameraData, float& Surface, float& ReprojectionError)
{
	Surface = -1;
	return Affine3d::Identity();
}

///////////////////////////////
// SECTION Camera
///////////////////////////////

void Camera::RescaleFrames()
{
	BufferedFrame& buff = FrameBuffer;
	#ifdef WITH_CUDA
	if (!buff.FrameUndistorted.MakeGPUAvailable())
	{
		return;
	}
	#else
	if (!buff.FrameUndistorted.MakeCPUAvailable())
	{
		return;
	}
	#endif
	Size rescale = GetArucoReduction();

	#ifdef WITH_CUDA
	cuda::cvtColor(buff.FrameUndistorted.GPUFrame, buff.GrayFrame.GPUFrame, buff.FrameUndistorted.GPUFrame.channels() == 3 ? COLOR_BGR2GRAY : COLOR_BGRA2GRAY);
	if (rescale == buff.FrameUndistorted.GPUFrame.size())
	{
		buff.RescaledFrame = MixedFrame(buff.GrayFrame.GPUFrame);
	}
	else
	{
		cuda::resize(buff.GrayFrame.GPUFrame, buff.RescaledFrame.GPUFrame, rescale, 0, 0, INTER_AREA);
	}
	buff.GrayFrame.HasGPU = true;
	buff.GrayFrame.HasCPU = false;
	buff.RescaledFrame.HasGPU = true;
	buff.RescaledFrame.HasCPU = false;
	#else
	bool rgb = buff.FrameUndistorted.CPUFrame.channels() == 3;
	cvtColor(buff.FrameUndistorted.CPUFrame, buff.GrayFrame.CPUFrame, rgb ? COLOR_BGR2GRAY : COLOR_BGRA2GRAY);
	
	if (rescale == buff.FrameUndistorted.CPUFrame.size())
	{
		buff.RescaledFrame = MixedFrame(buff.GrayFrame.CPUFrame);
	}
	else
	{
		resize(buff.GrayFrame.CPUFrame, buff.RescaledFrame.CPUFrame, rescale, 0, 0, INTER_AREA);
	}
	buff.GrayFrame.HasCPU = true;
	buff.RescaledFrame.HasCPU = true;
	#endif
}

void Camera::detectMarkers(aruco::ArucoDetector& Detector)
{
	BufferedFrame& buff = FrameBuffer;

	Size framesize = Settings.Resolution;
	Size rescaled = GetArucoReduction();

	if(!buff.GrayFrame.MakeCPUAvailable())
	{
		return;
	}
	if(!buff.RescaledFrame.MakeCPUAvailable())
	{
		return;
	}
	

	vector<vector<Point2f>> &corners = buff.markerCorners;
	corners.clear();
	vector<int> &IDs = buff.markerIDs;
	IDs.clear();
	buff.reprojectedCorners.clear();

	Detector.detectMarkers(buff.RescaledFrame.CPUFrame, corners, IDs);

	buff.reprojectedCorners.resize(IDs.size());

	Size2d scalefactor((double)framesize.width/rescaled.width, (double)framesize.height/rescaled.height);

	float reductionFactors = GetReductionFactor();

	if (framesize != rescaled)
	{
		//rescale corners to full image position
		for (int j = 0; j < corners.size(); j++)
		{
			for (size_t k = 0; k < 4; k++)
			{
				Point2f& corner = corners[j][k];
				corner.x *= scalefactor.width;
				corner.y *= scalefactor.height;
			}
		}

		//subpixel refinement
		UMat &framegray = buff.GrayFrame.CPUFrame;

		for (int ArucoIdx = 0; ArucoIdx < IDs.size(); ArucoIdx++)
		{
			Size window = Size(reductionFactors, reductionFactors);
			cornerSubPix(framegray, corners[ArucoIdx], window, Size(-1,-1), TermCriteria(TermCriteria::COUNT | TermCriteria::EPS, 100, 0.01));
		}
	}
	buff.Status.HasAruco = true;
}

bool Camera::GetMarkerData(CameraArucoData& CameraData)
{
	BufferedFrame& buff = FrameBuffer;
	if (!buff.Status.HasAruco)
	{
		return false;
	}
	CameraData.TagIDs = buff.markerIDs;
	CameraData.TagCorners = buff.markerCorners;
	GetCameraSettingsAfterUndistortion(CameraData.CameraMatrix, CameraData.DistanceCoefficients);
	CameraData.CameraTransform = Location;
	CameraData.SourceCamera = this;
	return true;
}

void Camera::SetMarkerReprojection(int MarkerIndex, const vector<cv::Point2d> &Corners)
{
	FrameBuffer.reprojectedCorners[MarkerIndex] = Corners;
}*/