#include <PostProcessing/YoloDeflicker.hpp>
#include <EntryPoints/CDFRExternal.hpp>
#include <iostream>

using namespace std;

bool PostProcessYoloDeflicker::IsYolo(const ObjectData& obj)
{
	return obj.type >= ObjectType::Fragile && obj.type <= ObjectType::PottedPlant;
}

void PostProcessYoloDeflicker::Process(vector<CameraImageData> &ImageData, vector<CameraFeatureData> &FeatureData, vector<ObjectData> &Objects)
{
	(void) ImageData;
	(void) FeatureData;
	for (auto &cacheobj : CachedObjects)
	{
		cacheobj.Associated = false;
	}
	int numnew=0, numlinked=0;
	for (auto &obj : Objects)
	{
		if (!IsYolo(obj))
		{
			continue;
		}
		bool associated = false;
		for (auto &cacheobj : CachedObjects)
		{
			if (cacheobj == obj)
			{
				cacheobj+=obj;
				associated = true;
				numlinked++;
				break;
			}
		}
		if (associated)
		{
			continue;
		}
		CachedObjects.emplace_back(obj);
		numnew++;
	}
	auto time_threshold = ObjectData::Clock::now();
	auto cache_erase_iterator = std::remove_if(CachedObjects.begin(), 
		CachedObjects.end(),
		[time_threshold](YoloObject &obj) { return !obj.Associated && (obj.Lifetime < time_threshold); });
	//cout << numnew << " new in cache, " << numlinked << " relinked, " << CachedObjects.end() - cache_erase_iterator << " to be deleted, ";
	CachedObjects.erase(cache_erase_iterator, CachedObjects.end());
	//cout << CachedObjects.size() << " still in cache" << endl;
	//cout << "Vector before yolo erase: " << Objects.size() <<endl;
	Objects.erase(
		std::remove_if(Objects.begin(), 
			Objects.end(),
			&PostProcessYoloDeflicker::IsYolo),
		Objects.end());
	//cout << "Vector after yolo erase: " << Objects.size() <<endl;
	Objects.insert(Objects.end(), CachedObjects.begin(), CachedObjects.end());
	//cout << "Vector after re-adding: " << Objects.size() <<endl;
}