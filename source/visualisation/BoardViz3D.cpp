#include "visualisation/BoardViz3D.hpp"
#include <opencv2/viz.hpp>
#include <opencv2/core.hpp>
#include <opencv2/core/affine.hpp>

#include "GlobalConf.hpp"
#include "TrackedObjects/TrackedObject.hpp"
#include "Camera.hpp"
#include "math3d.hpp"

const String assetpath = "../assets/";

bool BoardViz3D::MeshesLoaded;
viz::Mesh BoardViz3D::BoardMesh, BoardViz3D::RobotMesh;
Mat BoardViz3D::BoardMat;

BoardViz3D::BoardViz3D()
{
}

BoardViz3D::~BoardViz3D()
{
}

void BoardViz3D::LoadMeshes()
{
	if (!MeshesLoaded)
	{
		BoardMesh = viz::Mesh::load(assetpath + "board.obj", viz::Mesh::LOAD_OBJ);
		BoardMat = imread(assetpath + "VisuelTable.png");
		MeshesLoaded = true;
	}
	
}

void BoardViz3D::SetupTerrain(viz::Viz3d& Visualizer)
{
	LoadMeshes();
	Visualizer.showWidget("axis", viz::WCoordinateSystem(0.25));
	Visualizer.showWidget("BoardMesh", viz::WMesh(BoardMesh));
	viz::WImage3D tableImg(BoardMat, Size2d(3, 2));
	//Visualizer.setRenderingProperty("BoardMesh", viz::REPRESENTATION, viz::REPRESENTATION_WIREFRAME);
	Visualizer.showWidget("BoardMat", tableImg, ImageToWorld());
	//Visualizer.setFullScreen(true);
	Visualizer.setViewerPose(Affine3d(MakeRotationFromZX(Vec3d(0,0,-1), Vec3d(1,0,0)), Vec3d(0,0,4)));
}

void BoardViz3D::ShowCamera(viz::Viz3d& Visualizer, Camera* Camera, int BufferIdx, Affine3d Pose, viz::Color color)
{
	viz::WCameraPosition CamWidget;
	if (BufferIdx != -1 && false)
	{
		UMat Frame;Camera->GetOutputFrame(BufferIdx, Frame, Size(640,480));
		CamWidget = viz::WCameraPosition((Matx33d)(Camera->CameraMatrix), Frame, 1.0, color);
	}
	else
	{
		//CamWidget = viz::WCameraPosition(Vec2d(Camera->CameraMatrix.at<double>(0,0), Camera->CameraMatrix.at<double>(1,1)), 0.2, color);
		CamWidget = viz::WCameraPosition((Matx33d)(Camera->CameraMatrix), 0.2, color);
	}
	Visualizer.showWidget(Camera->GetDevicePath(), CamWidget, Pose);
	return;
}

viz::Mesh BoardViz3D::GetBoardMesh()
{
	LoadMeshes();
	return BoardMesh;
}

viz::Mesh BoardViz3D::GetRobotMesh()
{
	LoadMeshes();
	return RobotMesh;
}

Mat BoardViz3D::GetBoardMat()
{
	LoadMeshes();
	return BoardMat;
}

Affine3d BoardViz3D::ImageToWorld()
{
	return Affine3d(Matx33d(1,0,0, 0,-1,0, 0,0,1), Vec3d::all(0));
}


void TestViz3D()
{
	viz::Viz3d board("3D Board");
	BoardViz3D::SetupTerrain(board);
	board.spin();
	exit(EXIT_SUCCESS);
}