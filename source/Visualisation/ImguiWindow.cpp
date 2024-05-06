
#include "Visualisation/ImguiWindow.hpp"

#include <Misc/path.hpp>
#include <Misc/math2d.hpp>
#include <EntryPoints/CDFRExternal.hpp>
#include <EntryPoints/CDFRCommon.hpp>
#include <Cameras/ImageTypes.hpp>
#include <Visualisation/BoardGL.hpp>

#include <thirdparty/HsvConverter.h>

#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <imgui.h>
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <opencv2/core.hpp>

#include <string>
#include <iostream>
#include <map>

using namespace std;

bool ImguiWindow::ImguiOpenGLInit = false;
string ImguiWindow::ImguiIniPath = "";

ImguiWindow::ImguiWindow(string InWindowName, CDFRExternal *InParent)
	:Task(), 
	WindowName(InWindowName), Parent(InParent)
{
	if (Parent)
	{
		Start();
	}
	else
	{
		Init();
	}
}

ImguiWindow::~ImguiWindow()
{
	for (auto &i : Textures)
	{
		i.Release();
	}
	
	cout << "Deleting ImGui" << endl;
	//ImGui_ImplGlfw_Shutdown();
}

void ImguiWindow::WindowSizeCallback(int width, int height)
{
	//glViewport(0, 0, width, height);
	ImGui::GetIO().DisplaySize = ImVec2(width, height);
}

ImVec2 ImguiWindow::GetWindowSize()
{
	if (!Window)
	{
		return ImVec2(0,0);
	}
	int winwidth, winheight;
	glfwGetWindowSize(Window, &winwidth, &winheight);
	return ImVec2(winwidth, winheight);
}

void ImguiWindow::Init()
{
	cout << "Creating ImGui window" << endl;
	GLCreateWindow(1280, 720, WindowName);
	if (!Window)
	{
		return;
	}

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;	 // Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;	  // Enable Gamepad Controls
	ImguiIniPath = GetCyclopsPath() / "build" / "imgui.ini";
	io.IniFilename = ImguiIniPath.c_str();
	
	// Setup Dear ImGui style
	ImGui::StyleColorsDark();
	//ImGui::StyleColorsLight();

	// Setup Platform/Renderer backends
	ImGui_ImplGlfw_InitForOpenGL(Window, true);
	if (ImguiOpenGLInit)
	{
		return;
	}
	ImguiOpenGLInit = true;
	ImGui_ImplOpenGL3_Init(nullptr);
}

void ImguiWindow::StartFrame()
{
	if (!Window)
	{
		return;
	}
	glfwMakeContextCurrent(Window);
	// Poll and handle events (inputs, window resize, etc.)
	// You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
	// - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
	// - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
	// Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
	glfwPollEvents();

	// Start the Dear ImGui frame
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
}

bool ImguiWindow::EndFrame()
{
	if (!Window)
	{
		return true;
	}
	// Rendering
	ImGui::Render();
	glClearColor(0,0,0,0);
	glClear(GL_COLOR_BUFFER_BIT);
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

	glfwSwapBuffers(Window);

	bool IsDone = glfwWindowShouldClose(Window) == 0;
	glfwMakeContextCurrent(NULL);

	return IsDone;
}

void ImguiWindow::AddImageToBackground(const Texture &Image, cv::Rect impos, cv::Size2f UVmin, cv::Size2f UVmax)
{
	if(!Window)
	{
		return;
	}
	ImDrawList* background = ImGui::GetBackgroundDrawList();
	{
		ImVec2 p_min = impos.tl(), p_max = impos.br();
		
		background->AddImage((void*)(intptr_t)Image.GetTextureID(), p_min, p_max, ImVec2(UVmin), ImVec2(UVmax));
	}
}

void ImguiWindow::ThreadEntryPoint()
{
	Init();
	//FrameCounter fps;
	vector<UMatData*> LastMatrices;
	while (!killed && Parent && !Parent->IsKilled())
	{
		//cout << "Direct Visualizer FPS :" << 1.0/fps.GetDeltaTime() << endl;
		StartFrame();
		int DisplaysPerCam = 1;
		auto Cameras = Parent->GetImage();
		auto Features = Parent->GetFeatureData();
		int NumDisplays = Cameras.size()*DisplaysPerCam;
		if ((int)Textures.size() != NumDisplays)
		{
			Textures.resize(NumDisplays);
			LastMatrices.resize(NumDisplays, nullptr);
		}
		cv::Size WindowSize = GetWindowSize();
		cv::Size ImageSize = WindowSize;
		if (Cameras.size() > 0)
		{
			ImageSize = Cameras[0].Image.size();
		}

		if (ImGui::Begin("Settings"))
		{
			ImGui::Text("%.1f fps", 1.0/Parent->DetectionFrameCounter.GetLastDelta());
			if (!Parent->OpenGLBoard)
			{
				if (ImGui::Button("Open 3D vizualiser"))
				{
					Parent->Open3DVisualizer();
				}
			}
			else
			{
				if (ImGui::Button("Close 3D vizualiser"))
				{
					Parent->OpenGLBoard.reset();
				}
			}
			
			if (ImGui::Button("Close this window"))
			{
				killed=true;
			}
			ImGui::Checkbox("Solve Camera Location", &CDFRCommon::ExternalSettings.SolveCameraLocation);

			map<const char *, CDFRCommon::Settings&> settingsmap({{"External", CDFRCommon::ExternalSettings}, {"Internal", CDFRCommon::InternalSettings}});
			Parent->ForceRecordNext = ImGui::Button("Capture next frame");

			for (auto &entry : settingsmap)
			{
				if (!ImGui::CollapsingHeader(entry.first))
				{
					continue;
				}
				ImGui::InputInt("Record interval", &entry.second.RecordInterval);
				if(ImGui::Checkbox("Record", &entry.second.record))
				{
				}
				//ImGui::Checkbox("Freeze camera position", nullptr);
				ImGui::Checkbox("Aruco Detection", &entry.second.ArucoDetection);
				ImGui::Checkbox("Distorted detection", &entry.second.DistortedDetection);
				ImGui::Checkbox("Segmented detection", &entry.second.SegmentedDetection);
				ImGui::Checkbox("POI Detection", &entry.second.POIDetection);
				ImGui::Checkbox("Yolo detection", &entry.second.YoloDetection);
				ImGui::Checkbox("Denoising", &entry.second.Denoising);
				ImGui::Spacing();
			}
			
			
			ImGui::Checkbox("Idle", &Parent->Idle);

			ImGui::Checkbox("Show Aruco", &ShowAruco);
			ImGui::Checkbox("Show Yolo", &ShowYolo);

			ImGui::Checkbox("Focus peeking", &FocusPeeking);
			
		}
		ImGui::End();

		
		
		
		auto tiles = DistributeViewports(ImageSize, WindowSize, NumDisplays);
		assert(tiles.size() == Cameras.size());
		//show cameras and arucos
		for (size_t camidx = 0; camidx < Cameras.size(); camidx++)
		{
			auto &thisTile = tiles[camidx*DisplaysPerCam];
			const auto &ImData = Cameras[camidx];
			const auto &FeatData = Features[camidx];
			Size Resolution = ImData.Image.size();
			if (FocusPeeking)
			{
				auto POIs = Parent->UnknownTracker.GetPointsOfInterest();
				auto POIRects = GetPOIRects(POIs, Resolution, FeatData.CameraTransform, 
					ImData.CameraMatrix, ImData.DistanceCoefficients);
				auto POI = POIRects[POIs.size()/2];
				thisTile.height = ImageSize.height;
				thisTile.width = ImageSize.width;
				thisTile.x = -POI.x+(WindowSize.width-POI.width)/2;
				thisTile.y = -POI.y+(WindowSize.height-POI.height)/2;
			}
			if (LastMatrices[camidx*DisplaysPerCam] != ImData.Image.u)
			{
				LastMatrices[camidx*DisplaysPerCam] = ImData.Image.u;
				Textures[camidx*DisplaysPerCam].LoadFromUMat(ImData.Image);
			}
			
			AddImageToBackground(Textures[camidx*DisplaysPerCam], thisTile);

			auto DrawList = ImGui::GetForegroundDrawList();
			{
				ostringstream CameraTextStream;
				CameraTextStream << FeatData.CameraTransform.translation() <<endl;
				CameraTextStream << Parent->GetTeam() << endl;
				string CameraText = CameraTextStream.str();
				DrawList->AddText(NULL, 12, ImVec2(thisTile.x, thisTile.y), IM_COL32(255,255,255,255), CameraText.c_str());
			}
			Rect SourceRemap(Point(0,0), Resolution);
			Rect DestRemap = tiles[camidx];
			//draw aruco
			if (ShowAruco && !FocusPeeking)
			{
				for (size_t arucoidx = 0; arucoidx < FeatData.ArucoIndices.size(); arucoidx++)
				{
					auto corners = FeatData.ArucoCorners[arucoidx];
					uint32_t color = IM_COL32(255, 128, 255, 128);
					if (FeatData.ArucoCornersReprojected[arucoidx].size() != 0)
					{
						//cout << arucoidx << " is reprojected" << endl;
						corners = FeatData.ArucoCornersReprojected[arucoidx];
						color = IM_COL32(128, 255, 255, 128);
					}
					
					Point2d textpos(0,0);
					for (auto cornerit = corners.cbegin(); cornerit != corners.cend(); cornerit++)
					{
						auto vizpos = ImageRemap<double>(SourceRemap, DestRemap, *cornerit);
						textpos.x = max<double>(textpos.x, vizpos.x);
						textpos.y = max<double>(textpos.y, vizpos.y);
						if (cornerit == corners.begin())
						{
							//corner0 square
							Point2d size = Point2d(2,2);
							DrawList->AddRect(vizpos-size, vizpos+size, color);
						}
						//start aruco contour
						DrawList->PathLineTo(vizpos);
					}
					//finish aruco contour
					DrawList->PathStroke(color, ImDrawFlags_Closed, 2);
					//text with aruco number
					string text = to_string(FeatData.ArucoIndices[arucoidx]);
					DrawList->AddText(nullptr, 16, textpos, color, text.c_str());
				}
				//display segments of segmented detection
				for (size_t segmentidx = 0; segmentidx < FeatData.ArucoSegments.size(); segmentidx++)
				{
					auto &segment = FeatData.ArucoSegments[segmentidx];
					auto tl = ImageRemap<double>(SourceRemap, DestRemap, segment.tl());
					auto br = ImageRemap<double>(SourceRemap, DestRemap, segment.br());
					DrawList->AddRect(tl, br, IM_COL32(255, 255, 255, 64));
				}
			}
			if (ShowYolo && !FocusPeeking)
			{
				for (size_t detidx = 0; detidx < FeatData.YoloDetections.size(); detidx++)
				{
					auto det = FeatData.YoloDetections[detidx];
					uint8_t r,g,b;
					HsvConverter::getRgbFromHSV(1530*det.Class/Parent->YoloDetector->GetNumClasses(), 255, 255, r, g, b);
					uint32_t color = IM_COL32(r,g,b,det.Confidence*255);
					
					Point2d textpos(0,0);
					auto tl = ImageRemap<double>(SourceRemap, DestRemap, det.Corners.tl());
					auto br = ImageRemap<double>(SourceRemap, DestRemap, det.Corners.br());
					DrawList->AddRect(tl, br, color);
					//text with class and confidence
					string text = Parent->YoloDetector->GetClassName(det.Class) + string("\n") + to_string(int(det.Confidence*100));
					DrawList->AddText(nullptr, 16, tl, color, text.c_str());
				}
			}
			
		}
		if(!EndFrame())
		{
			killed = true;
			closed = true;
		}
	}
}