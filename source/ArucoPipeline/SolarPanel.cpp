

#include <ArucoPipeline/SolarPanel.hpp>

#include <opencv2/calib3d.hpp>
#include <Misc/math3d.hpp>

using namespace std;
using namespace cv;

SolarPanel::SolarPanel()
	:TrackedObject()
{
	markers = {
		ArucoMarker(37.5/1000.0, 47)
	};
	Unique=true;
	Name="Solar Panels";

	const double panelY=-1.037;
	const double panelGroupSeparationX=1.0, panelSeparationX=0.225;
	const double ExpectedZ = 0.104;
	int panelindex = 0;
	for (int i = -1; i <=1; i++)
	{
		for (int j = -1; j <= 1; j++)
		{
			double panelX=i*panelGroupSeparationX+j*panelSeparationX;
			PanelPositions[panelindex] = Point3d(panelX, panelY, ExpectedZ);
			panelindex++;
		}
	}
}

Affine3d SolarPanel::GetObjectTransform(const CameraFeatureData& CameraData, float& Surface, float& ReprojectionError, 
	map<int, vector<Point2f>> &ReprojectedCorners)
{
	(void) ReprojectedCorners;

	std::vector<ArucoViewCameraLocal> SeenMarkers;
	Surface = GetSeenMarkers(CameraData, SeenMarkers);
	auto &flatobj = markers[0].GetObjectPointsNoOffset();
	ReprojectionError = 0;
	Affine3d WorldToCam = CameraData.CameraTransform.inv();
	vector<Point2d> ImageSolarPanels;
	projectPoints(PanelPositions, WorldToCam.rvec(), WorldToCam.translation(), CameraData.CameraMatrix, CameraData.DistanceCoefficients, ImageSolarPanels);
	for (auto &marker : SeenMarkers)
	{
		auto& flatimg = marker.CameraCornerPositions;

		Point2d AimPos = (flatimg[0]+flatimg[1])/2.0;
		int closest = -1;
		double smallestDistance = INFINITY;
		for (size_t i = 0; i < ImageSolarPanels.size(); i++)
		{
			auto &PanelLocation = ImageSolarPanels[i];
			auto deltapos = PanelLocation-AimPos;
			double deltasq = deltapos.dot(deltapos);
			if (deltasq < smallestDistance)
			{
				closest = (int)i;
				smallestDistance = deltasq;
			}
		}
		smallestDistance = sqrt(smallestDistance);
		double LocationTolerance = arcLength(flatimg, true); //in pixels
		
		if (smallestDistance>LocationTolerance)
		{
			//cout << "Distance to closest (" << closest << ") too big " << smallestDistance << " (closest panel is at " << ImageSolarPanels[closest] << ", seen is at " << AimPos << ") rejected." << endl;
			continue;
		}
		
		Mat rvec = Mat::zeros(3, 1, CV_64F), tvec = Mat::zeros(3, 1, CV_64F);
		try
		{
			solvePnP(flatobj, flatimg, CameraData.CameraMatrix, CameraData.DistanceCoefficients, rvec, tvec, false, SOLVEPNP_IPPE_SQUARE);
		}
		catch(const std::exception& e)
		{
			std::cerr << e.what() << '\n';
			continue;
		}
		solvePnPRefineLM(flatobj, flatimg, CameraData.CameraMatrix, CameraData.DistanceCoefficients, rvec, tvec);

		Matx33d rotationMatrix; //Matrice de rotation Camera -> Tag
		Rodrigues(rvec, rotationMatrix);

		Affine3d localTransform = Affine3d(rotationMatrix, tvec); //Camera -> Tag
		Affine3d WorldTransform = CameraData.CameraTransform * localTransform;

		PanelRotations[closest] = GetRotZ(WorldTransform.rotation());
		//cout << "Panel " << closest << " has a rotation of " << PanelRotations[closest]*180.0/M_PI << " deg" << endl;
		Affine3d ExactTransform(rotationMatrix, PanelPositions[closest]);
	}
	return Affine3d::Identity();
}

vector<ObjectData> SolarPanel::ToObjectData() const
{
	vector<ObjectData> objects;
	for (size_t i = 0; i < PanelPositions.size(); i++)
	{
		double rot = PanelRotations[i];
		Affine3d location(MakeRotationFromZX(Vec3d(0,0,1), Vec3d(cos(rot),sin(rot),0)), PanelPositions[i]);
		objects.emplace_back(ObjectType::SolarPanel, "Solar Panel " + to_string(i), location);
	}
	return objects;
}