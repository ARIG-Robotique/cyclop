#include <string>
#include <opencv2/core.hpp>
#include <fstream>


static bool readCameraParameters(std::string filename, Mat& camMatrix, Mat& distCoeffs, Size Resolution)
{
	cv::FileStorage fs(filename, cv::FileStorage::READ);
	if (!fs.isOpened())
		return false;
	
	Mat1i resmat; Size CalibRes;
	fs["resolution"] >> resmat;
	CalibRes.width = resmat.at<int>(0,0);
	CalibRes.height = resmat.at<int>(0,1);
	Mat calibmatrix;
	fs["camera_matrix"] >> calibmatrix;
	fs["distortion_coefficients"] >> distCoeffs;
	Mat scalingMatrix = Mat::zeros(3,3, CV_64F);
	scalingMatrix.at<double>(0,0) = Resolution.width / CalibRes.width;
	scalingMatrix.at<double>(1,1) = Resolution.height / CalibRes.height;
	scalingMatrix.at<double>(2,2) = 1;
	camMatrix = scalingMatrix * calibmatrix;
	return (camMatrix.size() == cv::Size(3,3));
}

static void writeCameraParameters(std::string filename, Mat camMatrix, Mat distCoeffs, Size Resolution)
{
	cv::FileStorage fs(filename, cv::FileStorage::WRITE);
	if (!fs.isOpened())
		return;
	Mat1i resmat(1,2);
	resmat.at<int>(0,0) = Resolution.width;
	resmat.at<int>(0,1) = Resolution.height;
	fs.write("resolution", resmat);
	fs.write("camera_matrix", camMatrix);
	fs.write("distortion_coefficients", distCoeffs);
}