#include "Scenarios/CDFRInternal.hpp"
#include "Scenarios/CDFRCommon.hpp"

void CDFRInternalMain(bool direct)
{
    vector<Camera*> physicalCameras = autoDetectCameras(CameraStartType::GSTREAMER_CPU, "", "Brio");
    Ptr<aruco::Dictionary> dictionary = GetArucoDict();
	
	viz::Viz3d board3d("local robot");
	BoardViz3D::SetupRobot(board3d);

    StartCameras(physicalCameras);

	cout << "Start grabbing " << physicalCameras.size() << " physical" << endl
		<< "Press ESC to terminate" << endl;

	
	Ptr<aruco::DetectorParameters> parameters = GetArucoParams();
	FrameCounter fps;
	int PipelineIdx = 0;
	
	vector<OutputImage*> OutputTargets;
	if (direct)
	{
        namedWindow("Cameras", WINDOW_NORMAL);
		setWindowProperty("Cameras", WND_PROP_FULLSCREEN, WINDOW_FULLSCREEN);
		
		for (int i = 0; i < physicalCameras.size(); i++)
		{
			OutputTargets.push_back(physicalCameras[i]);
		}
	}

	ObjectTracker tracker;
	tracker.SetArucoSize(center.number, center.sideLength);

    vector<String> SerialPorts = SerialSender::autoDetectTTYUSB();
    for (size_t i = 0; i < SerialPorts.size(); i++)
    {
        cout << "Serial port found at " << SerialPorts[i] << endl;
    }
    
	serialib* bridge = new serialib();
    if (SerialPorts.size() > 0)
    {
        int success = bridge->openDevice(SerialPorts[0].c_str(), SerialTransmission::BaudRate);
        cout << "Result opening serial bridge : " << success << endl;
		if (success != 1)
		{
			cout << "Failed to open the serial bridge, make sure your user is in the dialout group" <<endl;
			cout << "run this ->   sudo usermod -a -G dialout $USER    <- then restart your PC." << endl;
		}
		
    }
    
	SerialSender sender(bridge);
	TrackerCube* robot1 = new TrackerCube({51, 52, 54, 55}, 0.06, Point3d(0.0952, 0.0952, 0), "Robot1");
	TrackerCube* robot2 = new TrackerCube({57, 58, 59, 61}, 0.06, Point3d(0.0952, 0.0952, 0), "Robot2");
	tracker.RegisterTrackedObject(robot1);
	tracker.RegisterTrackedObject(robot2);

	sender.RegisterRobot(robot1);
	sender.RegisterRobot(robot2);

	ofstream printfile;
	printfile.open("logpos.csv", ios::out);

	sender.PrintCSVHeader(printfile);
	
	int hassent = 0;
	for (;;)
	{
		BufferedPipeline(PipelineIdx, physicalCameras, dictionary, parameters, &tracker);
		PipelineIdx = (PipelineIdx + 1) % Camera::FrameBufferSize;

		//cout << "Pipeline took " << TimePipeline << "s to run" << endl;
		
		vector<CameraView> views;
		vector<Affine3d> cameras;
		cameras.resize(physicalCameras.size());

		int viewsize = 0;
		for (int i = 0; i < physicalCameras.size(); i++)
		{
			viewsize += physicalCameras[i]->GetCameraViewsSize(PipelineIdx);
		}
		
		views.resize(viewsize);
		int viewsidx = 0;

		for (int i = 0; i < physicalCameras.size(); i++)
		{
			Camera* cam = physicalCameras[i];
			vector<CameraView> CameraViews;
			if (!cam->GetCameraViews(PipelineIdx, CameraViews))
			{
				continue;
			}
			Affine3d CamTransform = Affine3d::Identity();
			bool has42 = false;
			for (int mark = 0; mark < CameraViews.size(); mark++)
			{
				int markerid = CameraViews[mark].TagID;
				if (markerid == center.number)
				{
					CamTransform = center.Pose * CameraViews[mark].TagTransform.inv();
					cam->Location = CamTransform;
					has42 = true;
				}
				views[viewsidx++] = CameraViews[mark];
				
			}
			cameras[i] = cam->Location;
			BoardViz3D::ShowCamera(board3d, cam, PipelineIdx, cam->Location, has42 ? viz::Color::green() : viz::Color::red());
			//cout << "Camera" << i << " location : " << cam->Location.translation() << endl;
			
		}
		
		tracker.SolveLocations(cameras, views);
		tracker.DisplayObjects(&board3d);

		double deltaTime = fps.GetDeltaTime();

		if (direct)
		{
			UMat image = ConcatCameras(PipelineIdx, OutputTargets, OutputTargets.size());
			//board.GetOutputFrame(0, image, GetFrameSize());
			//cout << "Concat OK" <<endl;
			fps.AddFpsToImage(image, deltaTime);
			//printf("fps : %f\n", fps);
			imshow("Cameras", image);
		}
		
		
		viz::WText fpstext(to_string(1/deltaTime), Point2i(200,100));
		board3d.showWidget("fps", fpstext);
		board3d.spinOnce(1, true);
		sender.PrintCSV(printfile);

		const size_t rcvbuffsize = 1<<20;
		char rcvbuff[rcvbuffsize];
		if (!hassent)
		{
			hassent = 1;
			sender.SendPacket();
		}
		
		
		if (bridge->available() > 0)
		{
			int out = bridge->readString(rcvbuff, '\n', rcvbuffsize, 1);
			if (out > 0)
			{
				printf("Received from serial : %*s", out, rcvbuff);
			}
		}

		if (waitKey(1) == 27 || board3d.wasStopped())
		{
			break;
		}
	}
}