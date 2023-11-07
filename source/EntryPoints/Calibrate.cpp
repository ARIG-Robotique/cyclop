
#include "Calibrate.hpp"

#include <iostream> // for standard I/O
#include <string>   // for strings
#include <sstream>  // string to number conversion

#include <filesystem>
#include <thread>
#include <mutex>

#include <opencv2/core.hpp>		// Basic OpenCV structures (cv::Mat, Scalar)
#include <opencv2/videoio.hpp>
#include <opencv2/highgui.hpp>  // OpenCV window I/O
#include <opencv2/aruco.hpp>
#include <opencv2/calib3d.hpp>
#include <opencv2/imgproc.hpp>

#include "Misc/math2d.hpp"
#include "Misc/math3d.hpp"

#include "Visualisation/ImguiWindow.hpp"
#include "Visualisation/openGL/Texture.hpp"

#include "thirdparty/serialib.h"
#include "GlobalConf.hpp"
#include "Cameras/Camera.hpp"
#include "Cameras/VideoCaptureCamera.hpp"
#include "Cameras/Calibfile.hpp"
#include "Misc/FrameCounter.hpp"

using namespace std;
using namespace cv;
namespace fs = std::filesystem;

const string TempImgPath = "TempCalib";
const string CalibWindowName = "Calibration";


void CreateKnownBoardPos(Size BoardSize, float squareEdgeLength, vector<Point3f>& corners)
{
	for (int i = 0; i < BoardSize.height; i++)
	{
		for (int j = 0; j < BoardSize.width; j++)
		{
			corners.push_back(Point3f(j * squareEdgeLength, i * squareEdgeLength, 0));
		}   
	}
}

void CameraCalibration(vector<vector<Point2f>> CheckerboardImageSpacePoints, vector<string> ImagePaths, Size BoardSize, Size resolution, float SquareEdgeLength, Mat& CameraMatrix, Mat& DistanceCoefficients, Camera* CamToCalibrate)
{
	vector<vector<Point3f>> WorldSpaceCornerPoints(1);
	CreateKnownBoardPos(BoardSize, SquareEdgeLength, WorldSpaceCornerPoints[0]);
	WorldSpaceCornerPoints.resize(CheckerboardImageSpacePoints.size(), WorldSpaceCornerPoints[0]);

	Size FrameSize = resolution;
	vector<Mat> rVectors, tVectors;
	CameraMatrix = Mat::eye(3, 3, CV_64F);
	DistanceCoefficients = Mat::zeros(Size(4,1), CV_64F);
	CameraMatrix = initCameraMatrix2D(WorldSpaceCornerPoints, CheckerboardImageSpacePoints, FrameSize);
	cout << "Camera matrix at start : " << CameraMatrix << endl;
	UMat undistorted;

	int numimagesstart = WorldSpaceCornerPoints.size();
	float threshold = GetCalibrationConfig().CalibrationThreshold;
	for (int i = 0; i < numimagesstart; i++)
	{
		int numimages = WorldSpaceCornerPoints.size();
		calibrateCamera(WorldSpaceCornerPoints, CheckerboardImageSpacePoints, FrameSize, CameraMatrix, DistanceCoefficients, rVectors, tVectors, 
		CALIB_RATIONAL_MODEL, TermCriteria(TermCriteria::COUNT, 50, DBL_EPSILON));
		vector<float> reprojectionErrors;
		reprojectionErrors.resize(numimages);
		int indexmosterrors = 0;
		for (int imageidx = 0; imageidx < numimages; imageidx++)
		{
			vector<Point2f> reprojected;

			projectPoints(WorldSpaceCornerPoints[imageidx], rVectors[imageidx], tVectors[imageidx], CameraMatrix, DistanceCoefficients, reprojected);
			reprojectionErrors[imageidx] = ComputeReprojectionError(CheckerboardImageSpacePoints[imageidx], reprojected) / reprojected.size();
			if (reprojectionErrors[indexmosterrors] < reprojectionErrors[imageidx])
			{
				indexmosterrors = imageidx;
			}
		}
		if (reprojectionErrors[indexmosterrors] < threshold)
		{
			cout << "Calibration done, error is " << reprojectionErrors[indexmosterrors] << "px/pt at most" << endl;
			cout << numimages << " images remain " << endl;
			break;
		}
		else
		{
			cout << "Ejecting index " << indexmosterrors << ", stored at " << ImagePaths[indexmosterrors] << " for " << reprojectionErrors[indexmosterrors] << endl;
			WorldSpaceCornerPoints[indexmosterrors] = WorldSpaceCornerPoints[numimages-1];
			CheckerboardImageSpacePoints[indexmosterrors] = CheckerboardImageSpacePoints[numimages-1];
			ImagePaths[indexmosterrors] = ImagePaths[numimages-1];

			WorldSpaceCornerPoints.resize(numimages-1);
			CheckerboardImageSpacePoints.resize(numimages-1);
			ImagePaths.resize(numimages-1);
		}
	}
	for (int i = 0; i < ImagePaths.size(); i++)
	{
		cout<< ImagePaths[i] << " remains"<< endl; 
	}
}

vector<String> GetPathsToCalibrationImages()
{
	vector<String> pathos;
	for (const auto & entry : fs::directory_iterator(TempImgPath))
	{
		pathos.push_back(entry.path().string());
		//cout << entry.path().string() << endl;
	}
	return pathos;
}

int GetCalibrationImagesLastIndex(vector<String> Pathes)
{
	int next = -1;
	for (int i = 0; i < Pathes.size(); i++)
	{
		String stripped = Pathes[i].substr(TempImgPath.length()+1, Pathes[i].length() - TempImgPath.length()-1 - String(".png").length());
		try
		{
			int thatidx = 0;
			sscanf(Pathes[i].c_str(), "%d", &thatidx);
			next = next < thatidx ? thatidx : next;
		}
		catch(const std::exception& e)
		{
			cout << "Failed to stoi " << stripped <<endl;
			//std::cerr << e.what() << '\n';
		}
	}
	return next;
}

Size ReadAndCalibrate(Mat& CameraMatrix, Mat& DistanceCoefficients, Camera* CamToCalibrate)
{
	auto calconf = GetCalibrationConfig();
	Size CheckerSize = calconf.NumIntersections;
	vector<String> pathes = GetPathsToCalibrationImages();
	size_t numpathes = pathes.size();
	vector<vector<Point2f>> savedPoints;
	savedPoints.resize(numpathes);
	vector<Size> resolutions;
	resolutions.resize(numpathes);
	vector<bool> valids;
	valids.resize(numpathes);
	parallel_for_(Range(0, numpathes), [&](const Range InRange)
	{
		for (size_t i = InRange.start; i < InRange.end; i++)
		{
			Mat frame = imread(pathes[i], IMREAD_GRAYSCALE);
			resolutions[i] = frame.size();
			vector<Point2f> foundPoints;
			bool found = findChessboardCorners(frame, CheckerSize, foundPoints, CALIB_CB_ADAPTIVE_THRESH | CALIB_CB_NORMALIZE_IMAGE);
			if (found)
			{
				TermCriteria criteria(TermCriteria::COUNT | TermCriteria::EPS, 100, 0.001);
				cornerSubPix(frame, foundPoints, Size(4,4), Size(-1,-1), criteria);
				//Scalar sharpness = estimateChessboardSharpness(frame, CheckerSize, foundPoints);
				savedPoints[i] = foundPoints;
				valids[i] = true;
			}
			else
			{
				valids[i] =false;
				cout << "Failed to find chessboard in image " << pathes[i] << " index " << i << endl;
			}
			
		}
	});

	cout << "Images are done loading, starting calibration..." <<endl;
	for (int i = numpathes - 1; i >= 0; i--)
	{
		if (!valids[i])
		{
			resolutions.erase(resolutions.begin() + i);
			savedPoints.erase(savedPoints.begin() + i);
			valids.erase(valids.begin() + i);
			pathes.erase(pathes.begin() + i);
		}
	}
	
	bool multires = false;
	vector<Size> sizes;
	for (size_t i = 0; i < numpathes; i++)
	{
		bool hasres = false;
		for (size_t j = 0; j < sizes.size(); j++)
		{
			if (sizes[j] == resolutions[i])
			{
				hasres = true;
				break;
			}
		}
		if (!hasres)
		{
			sizes.push_back(resolutions[i]);
		}
	}
	
	if (sizes.size() == 1)
	{
		UMat image = imread(pathes[0], IMREAD_COLOR).getUMat(AccessFlag::ACCESS_READ);
		CameraCalibration(savedPoints, pathes, CheckerSize, sizes[0], calconf.SquareSideLength/1000.f, CameraMatrix, DistanceCoefficients, CamToCalibrate);
		cout << "Calibration done ! Matrix : " << CameraMatrix << " / Distance Coefficients : " << DistanceCoefficients << endl;
		return sizes[0];
	}
	else if (sizes.size() == 0)
	{
		cout << "Il faut prendre des images pour pouvoir calibrer la caméra quand même..." << endl;
	}
	else
	{
		cerr << "ERROR : " << sizes.size() << " different resolutions were used in the calibration. That's fixable but fuck you." << endl;
		cerr << "-Cordialement, le trez 2021/2022 Robotronik (Gabriel Zerbib)" << endl;
		for (size_t i = 0; i < sizes.size(); i++)
		{
			cerr << "@" << sizes[i] <<endl;
			for (size_t j = 0; j < numpathes; j++)
			{
				cerr << " -" << pathes[j] <<endl;
			}
		}
	}
	return Size(0,0);
}

Camera* CamToCalib;
Mat CameraMatrix;
Mat distanceCoefficients;

thread *CalibrationThread;
bool Calibrating = false;
bool ShowUndistorted = false;

void CalibrationWorker()
{
	Calibrating = true;
	Size resolution = ReadAndCalibrate(CameraMatrix, distanceCoefficients, CamToCalib);
	if (CamToCalib->connected)
	{
		CamToCalib->SetCalibrationSetting(CameraMatrix, distanceCoefficients);
		if (resolution != CamToCalib->GetCameraSettings()->Resolution)
		{
			cerr << "WARNING : Resolution of the stored images isn't the same as the resolution of the live camera!" <<endl;
		}
		
	}
	else
	{
		VideoCaptureCameraSettings CamSett = *dynamic_cast<const VideoCaptureCameraSettings*>(CamToCalib->GetCameraSettings());

		CamSett.Resolution = resolution;
		CamSett.CameraMatrix = CameraMatrix;
		CamSett.distanceCoeffs = distanceCoefficients;
		CamSett.DeviceInfo.device_description = "NoCam";
		CamToCalib->SetCameraSetting(make_shared<VideoCaptureCameraSettings>(CamSett));
	}
	
	
	auto calconf = GetCalibrationConfig();
	double apertureWidth = calconf.SensorSize.width, apertureHeight = calconf.SensorSize.height, fovx, fovy, focalLength, aspectRatio;
	Point2d principalPoint;
	calibrationMatrixValues(CameraMatrix, CamToCalib->GetCameraSettings()->Resolution, apertureWidth, apertureHeight, fovx, fovy, focalLength, principalPoint, aspectRatio);
	cout << "Computed camera parameters for sensor of size " << apertureWidth << "x" << apertureHeight <<"mm :" << endl
	<< " fov:" << fovx << "x" << fovy << "°, focal length=" << focalLength << ", aspect ratio=" << aspectRatio << endl
	<< "Principal point @ " << principalPoint << endl;
	writeCameraParameters(CamToCalib->GetName(), CameraMatrix, distanceCoefficients, CamToCalib->GetCameraSettings()->Resolution);
	//distanceCoefficients = Mat::zeros(8, 1, CV_64F);
	ShowUndistorted = true;
	Calibrating = false;
}

bool docalibration(VideoCaptureCameraSettings CamSett)
{
	bool HasCamera = CamSett.IsValid();
	
	if (HasCamera)
	{
		CamToCalib = new VideoCaptureCamera(make_shared<VideoCaptureCameraSettings>(CamSett));
		CamToCalib->StartFeed();
	}
	else
	{
		CamToCalib = nullptr;
	}
	

	bool AutoCapture = false;
	float AutoCaptureFramerate = 2;
	double AutoCaptureStart;
	int LastAutoCapture;

	
	fs::create_directory(TempImgPath);

	//namedWindow(CalibWindowName, WINDOW_NORMAL);
	//setWindowProperty(CalibWindowName, WND_PROP_FULLSCREEN, WINDOW_FULLSCREEN);

	if (!HasCamera)
	{
		cout << "No camera was found, calibrating from saved images" << endl;
		CalibrationWorker();
		vector<String> pathes = GetPathsToCalibrationImages();
		for (int i = 0; i < pathes.size(); i++)
		{
			Mat image = imread(pathes[i]);
			UMat image2, undist;
			image.copyTo(image2);
			CamToCalib->Read();
			CamToCalib->Undistort();
			CameraImageData Frame;
			CamToCalib->GetFrame(Frame, false);
			imshow(CalibWindowName, undist);
			waitKey(1000);
		}
		
		return true;
	}
	CamToCalib->StartFeed();

	ImguiWindow imguiinst;
	Texture cameratex;
	cameratex.Texture = Mat::zeros(CamSett.Resolution, CV_8UC3);
	cameratex.valid = true;
	
	cameratex.Bind();


	cout << "Camera calibration mode !" << endl
	<< "Press [space] to capture an image, [enter] to calibrate, [a] to capture an image every " << 1/AutoCaptureFramerate << "s" <<endl
	<< "Take pictures of a checkerboard with " << GetCalibrationConfig().NumIntersections.width+1 << "x" << GetCalibrationConfig().NumIntersections.height+1 << " squares of side length " << GetCalibrationConfig().SquareSideLength << "mm" << endl
	<< "Images will be saved in folder " << TempImgPath << endl;

	
	//startWindowThread();
	
	vector<String> pathes = GetPathsToCalibrationImages();
	int nextIdx = GetCalibrationImagesLastIndex(pathes) +1;
	int64 lastCapture = getTickCount();

	FrameCounter fps;
	int failed = 0;
	bool CapturedImageLastFrame = false;
	bool mirrored = true;
	while (true)
	{
		imguiinst.StartFrame();
		ImGuiIO& IO = ImGui::GetIO();

		UMat frame;
		bool CaptureImageThisFrame = false;
		
		if (!CamToCalib->Read())
		{
			//cout<< "read fail" <<endl;
			failed++;
			if (failed >10)
			{
				break;
			}
			
			continue;
		}
		//cout << "read success" << endl;
		//drawChessboardCorners(drawToFrame, CheckerSize, foundPoints, found);
		//char character = waitKey(1);
		/*#ifdef IMGUI_HAS_VIEWPORT
		ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(viewport->GetWorkPos());
		ImGui::SetNextWindowSize(viewport->GetWorkSize());
		ImGui::SetNextWindowViewport(viewport->ID);
		#else 
		ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
		ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
		#endif
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::Begin("Background", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBringToFrontOnFocus);
		ImGui::End();
		ImGui::PopStyleVar(1);*/

		if (ImGui::Begin("Controls"))
		{
			ImGui::Text("FPS : %f", 1/fps.GetDeltaTime());
			ImGui::Checkbox("Mirror", &mirrored);
		}

		CameraImageData framedata;
		if (ShowUndistorted)
		{
			CamToCalib->Undistort();
		}
		CamToCalib->GetFrame(framedata, !ShowUndistorted);
		frame = framedata.Image;
		
		if (!ShowUndistorted)
		{
			if(ImGui::Checkbox("Auto capture", &AutoCapture))
			{
				//AutoCapture = !AutoCapture;
				AutoCaptureStart = fps.GetAbsoluteTime();
				LastAutoCapture = 0;
			}
			ImGui::SliderFloat("Auto capture framerate", &AutoCaptureFramerate, 1, 10);
		}
		if(ImGui::Button("Capture Image"))
		{
			//save image
			if (!ShowUndistorted)
			{
				CaptureImageThisFrame = true;
			}
		}
		if(ImGui::Button("Calibrate"))
		{
			if (Calibrating)
			{
				AutoCapture = false;
			}
			if (ShowUndistorted)
			{
				ShowUndistorted = false;
			}
			else
			{
				CalibrationThread = new thread(CalibrationWorker);
			}
		}

		if (AutoCapture)
		{
			int autoCaptureIdx = floor((fps.GetAbsoluteTime() - AutoCaptureStart)*AutoCaptureFramerate);
			if (autoCaptureIdx > LastAutoCapture)
			{
				LastAutoCapture++;
				CaptureImageThisFrame = true;
			}
			
		}
		if (CaptureImageThisFrame && !Calibrating && !ShowUndistorted)
		{
			vector<Point2f> foundPoints;
			UMat grayscale;
			cvtColor(frame, grayscale, COLOR_BGR2GRAY);
			//bool found = checkChessboard(grayscale, CheckerSize);
			imwrite(TempImgPath + "/" + to_string(nextIdx++) + ".png", frame);
			lastCapture = getTickCount() + getTickFrequency();
			CapturedImageLastFrame = true;
		}
		else
		{
			CapturedImageLastFrame = false;
		}
		
		if (Calibrating)
		{
			ImGui::Text("Calibrating, please wait...");
		}
		else if (getTickCount() < lastCapture)
		{
			ImGui::Text("Image %d %s !", nextIdx -1, ( AutoCapture ? "AutoCaptured" : "captured"));
		}
		
		ImGui::End();


		
		cameratex.Texture = frame.getMat(cv::AccessFlag::ACCESS_READ | cv::AccessFlag::ACCESS_FAST);
		cameratex.Refresh();
		cameratex.Texture = Mat();


		ImDrawList* background = ImGui::GetBackgroundDrawList();
		{
			cv::Rect Backgroundsize(Point2i(0,0), (Size2i)imguiinst.GetWindowSize());
			cv::Rect impos = ScaleToFit(CamSett.Resolution, Backgroundsize);
			ImVec2 p_min = impos.tl(), p_max = impos.br(), uv_min(0,0), uv_max(1,1);

			if (mirrored)
			{
				uv_min = ImVec2(1,0);
				uv_max = ImVec2(0,1);
			}
			
			background->AddImage((void*)(intptr_t)cameratex.GetTextureID(), p_min, p_max, uv_min, uv_max);
		}

		
		
		if(!imguiinst.EndFrame())
		{
			return true;
		}
		//fps.AddFpsToImage(frameresized, fps.GetDeltaTime());
		//imshow(CalibWindowName, frameresized);
	}
	return true;
}