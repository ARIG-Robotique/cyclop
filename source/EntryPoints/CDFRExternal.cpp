#include <EntryPoints/CDFRCommon.hpp>
#include <EntryPoints/CDFRExternal.hpp>

#include <DetectFeatures/ArucoDetect.hpp>

#include <Visualisation/BoardGL.hpp>
#include <Misc/ManualProfiler.hpp>

#include <Communication/Transport/TCPTransport.hpp>
#include <Communication/Transport/UDPTransport.hpp>
#include <thread>
#include <memory>

CDFRTeam GetTeamFromCameras(vector<Camera*> Cameras)
{
	if (Cameras.size() == 0)
	{
		return CDFRTeam::Unknown;
	}
	int blues = 0, greens = 0;
	const static double ydist = 1.1;
	const static double xdist = 1.45;
	const static map<CDFRTeam, vector<Vec2d>> CameraPos = 
	{
		{CDFRTeam::Blue, {{0, ydist}, {-xdist, -ydist}, {xdist, -ydist}, {-1.622, -0.1}}},
		{CDFRTeam::Yellow, {{0, -ydist}, {-xdist, ydist}, {xdist, ydist}, {-1.622, 0.1}}}
	};
	map<CDFRTeam, int> TeamScores;
	for (Camera* cam : Cameras)
	{
		auto campos = cam->GetLocation().translation();
		Vec2d pos2d(campos[0], campos[1]);
		CDFRTeam bestTeam = CDFRTeam::Unknown;
		double bestdist = 0.5; //tolerance of 50cm
		for (const auto [team, positions] : CameraPos)
		{
			for (const auto position : positions)
			{
				Vec2d delta = position-pos2d;
				double dist = sqrt(delta.ddot(delta));
				if (dist < bestdist)
				{
					bestTeam = team;
					bestdist = dist;
				}
			}
			
		}
		auto position = TeamScores.find(bestTeam);
		if (position == TeamScores.end())
		{
			TeamScores[bestTeam] = 1;
		}
		else
		{
			TeamScores[bestTeam] += 1;
		}
	}
	CDFRTeam bestTeam = CDFRTeam::Unknown;
	int mostCount = 0;
	for (const auto [team, count] : TeamScores)
	{
		if (count > mostCount)
		{
			bestTeam = team;
			mostCount = count;
		}
	}
	return bestTeam;
}

using ExternalProfType = ManualProfiler<false>;

void CDFRExternalMain(bool direct, bool v3d)
{
	ExternalProfType prof("External Global Profile");
	ExternalProfType ParallelProfiler("Parallel Cameras Detail");
	CameraManager CameraMan(GetCaptureMethod(), GetCaptureConfig().filter, false);

	auto& Detector = GetArucoDetector();

	//display/debug section
	FrameCounter fps;
	
	ObjectTracker bluetracker, yellowtracker;

	unique_ptr<StaticObject> boardobj = make_unique<StaticObject>(false, "board");
	bluetracker.RegisterTrackedObject(boardobj.get()); 
	yellowtracker.RegisterTrackedObject(boardobj.get());
	//TrackerCube* robot1 = new TrackerCube({51, 52, 54, 55}, 0.06, 0.0952, "Robot1");
	//TrackerCube* robot2 = new TrackerCube({57, 58, 59, 61}, 0.06, 0.0952, "Robot2");
	//bluetracker.RegisterTrackedObject(robot1);
	//bluetracker.RegisterTrackedObject(robot2);
	
	BoardGL OpenGLBoard;
	unique_ptr<TrackerCube> blue1 = make_unique<TrackerCube>(vector<int>({51, 52, 53, 54, 55}), 0.05, 85.065/1000.0, "blue1");
	unique_ptr<TrackerCube> blue2 = make_unique<TrackerCube>(vector<int>({56, 57, 58, 59, 60}), 0.05, 85.065/1000.0, "blue2");
	unique_ptr<TrackerCube> yellow1 = make_unique<TrackerCube>(vector<int>({71, 72, 73, 74, 75}), 0.05, 85.065/1000.0, "yellow1");
	unique_ptr<TrackerCube> yellow2 = make_unique<TrackerCube>(vector<int>({76, 77, 78, 79, 80}), 0.05, 85.065/1000.0, "yellow2");
	bluetracker.RegisterTrackedObject(blue1.get());
	bluetracker.RegisterTrackedObject(blue2.get());

	yellowtracker.RegisterTrackedObject(yellow1.get());
	yellowtracker.RegisterTrackedObject(yellow2.get());

	vector<unique_ptr<TopTracker>> TopTrackers;
	for (int i = 1; i < 11; i++)
	{
		TopTracker* tt = new TopTracker(i, 0.07, "top tracker " + std::to_string(i));
		TopTrackers.emplace_back(tt);
		bluetracker.RegisterTrackedObject(tt);
		yellowtracker.RegisterTrackedObject(tt);
	}
	
	
	//OpenGLBoard.InspectObject(blue1);
	if (v3d)
	{
		OpenGLBoard.Start();
		OpenGLBoard.LoadTags();
		OpenGLBoard.Tick({});
	}

	
	
	//track and untrack cameras dynamically
	CameraMan.StartCamera = [](VideoCaptureCameraSettings settings) -> Camera*
	{
		Camera* cam = new VideoCaptureCamera(make_shared<VideoCaptureCameraSettings>(settings));
		if(!cam->StartFeed())
		{
			cerr << "Failed to start feed @" << settings.DeviceInfo.device_description << endl;
			delete cam;
			return nullptr;
		}
		
		return cam;
	};
	CameraMan.RegisterCamera = [&bluetracker, &yellowtracker](Camera* cam) -> void
	{
		bluetracker.RegisterTrackedObject(cam);
		yellowtracker.RegisterTrackedObject(cam);
		cout << "Registering new camera @" << cam << endl;
	};
	CameraMan.StopCamera = [&bluetracker, &yellowtracker](Camera* cam) -> bool
	{
		bluetracker.UnregisterTrackedObject(cam);
		yellowtracker.UnregisterTrackedObject(cam);
		cout << "Unregistering camera @" << cam << endl;
		return true;
	};

	CameraMan.StartScanThread();

	int lastmarker = 0;
	CDFRTeam LastTeam = CDFRTeam::Unknown;
	for (;;)
	{

		double deltaTime = fps.GetDeltaTime();
		prof.EnterSection("CameraManager Tick");
		vector<Camera*> Cameras = CameraMan.Tick();
		CDFRTeam Team = GetTeamFromCameras(Cameras);
		if (Team != LastTeam)
		{
			const string teamname = TeamNames.at(Team);
			cout << "Detected team change : to " << teamname <<endl; 
			LastTeam = Team;
		}
		ObjectTracker* TrackerToUse = &bluetracker;
		if (Team == CDFRTeam::Yellow)
		{
			TrackerToUse = &yellowtracker;
		}
		
		prof.EnterSection("Camera Gather Frames");
		int64 GrabTick = getTickCount();
		
		for (int i = 0; i < Cameras.size(); i++)
		{
			Cameras[i]->Grab();
		}

		int NumCams = Cameras.size();
		vector<CameraFeatureData> FeatureData;
		vector<bool> CamerasWithPosition;
		vector<ExternalProfType> ParallelProfilers;
		FeatureData.resize(NumCams);
		CamerasWithPosition.resize(NumCams);
		ParallelProfilers.resize(NumCams);
		prof.EnterSection("Parallel Cameras");

		//grab frames
		//read frames
		//undistort
		//detect aruco and yolo

		parallel_for_(Range(0, Cameras.size()), 
		[&Cameras, &FeatureData, &CamerasWithPosition, TrackerToUse, GrabTick, &ParallelProfilers]
		(Range InRange)
		{
			for (int i = InRange.start; i < InRange.end; i++)
			{
				auto &thisprof = ParallelProfilers[i];
				thisprof.EnterSection("CameraRead");
				Camera* cam = Cameras[i];
				cam->Read();
				thisprof.EnterSection("CameraUndistort");
				cam->Undistort();
				thisprof.EnterSection("CameraGetFrame");
				CameraImageData ImData;
				cam->GetFrame(ImData, false);
				thisprof.EnterSection("DetectAruco");
				CameraFeatureData &FeatData = FeatureData[i];
				DetectAruco(ImData, FeatData);
				CamerasWithPosition[i] = TrackerToUse->SolveCameraLocation(FeatData);
				if (CamerasWithPosition[i])
				{
					cam->SetLocation(FeatData.CameraTransform, GrabTick);
				}
				FeatData.CameraTransform = cam->GetLocation();
			}
		});

		for (auto &pprof : ParallelProfilers)
		{
			ParallelProfiler += pprof;
		}

		int viewsidx = 0;

		prof.EnterSection("3D Solve");
		TrackerToUse->SolveLocationsPerObject(FeatureData, GrabTick);
		vector<ObjectData> ObjData = TrackerToUse->GetObjectDataVector(GrabTick);
		ObjectData TeamPacket(ObjectType::Team, TeamNames.at(Team));
		ObjData.insert(ObjData.begin(), TeamPacket); //insert team as the first object

		prof.EnterSection("Visualisation");
		
		if (v3d)
		{
			if(!OpenGLBoard.Tick(ObjectData::ToGLObjects(ObjData)))
			{
				break;
			}
		}
		
		prof.EnterSection("Send data");
		if (GetWebsocketConfig().Server)
		{
			//this_thread::sleep_for(chrono::milliseconds(1000));
		}
		
		
		prof.EnterSection("");
		
		
		
		if (prof.ShouldPrint())
		{
			cout << fps.GetFPSString(deltaTime) << endl;
			prof.PrintProfile();
			ParallelProfiler.PrintProfile();
		}
		
	}
}