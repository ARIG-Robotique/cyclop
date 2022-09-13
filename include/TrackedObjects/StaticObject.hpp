#pragma once

#include "TrackedObjects/TrackedObject.hpp"

class StaticObject : public TrackedObject
{
private:
	bool Relative; //Allow modifying the position of this object ?
public:
	StaticObject(bool InRelative, String InName);
	~StaticObject();

	virtual bool SetLocation(Affine3d InLocation) override;

	virtual vector<PositionPacket> ToPacket(int BaseNumeral) override;

	virtual void DisplayRecursive(viz::Viz3d* visualizer, Affine3d RootLocation, String rootName) override;
};