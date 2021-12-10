// This source code is property of the Computer Graphics and Visualization 
// chair of the TU Dresden. Do not distribute! 
// Copyright (C) CGV TU Dresden - All Rights Reserved

#pragma once

#include <cgv/render/drawable.h>

typedef cgv::render::drawable::vec3 vec3;

class Box
{
	//internal storage for lower and upper corner points of the box
	std::pair<vec3, vec3> bounds;

public:

	//creates an empty box like the method Clear
	Box();

	//construct a box with the gven lower and upper corner points
	Box(const vec3& lbound, const vec3& ubound);

	//returns the corner point which is the lower bound of the box in all dimensions
	vec3& LowerBound();

	//returns the corner point which is the lower bound of the box in all dimensions
	const vec3& LowerBound() const;

	//returns the corner point which is the upper bound of the box in all dimensions
	vec3& UpperBound();

	//returns the corner point which is the upper bound of the box in all dimensions
	const vec3& UpperBound() const;
	
	//returns a vector containing the extents of the box in all dimensions
	vec3 Extents() const;

	//returns a vector containing the extents of the box in all dimensions divided by 2
	vec3 HalfExtents() const;

	//returns the center of the box
	vec3 Center() const;

	//returns the surface area of the box
	float SurfaceArea() const;

	//returns the volume of the box
	float Volume() const;

	//returns the box radius for a given direction vector a 
	//if the box is centered at the origin 
	//then the projection of the box on direction a is contained within  the Interval [-r,r]
	float Radius(const vec3& a) const;

	//returns true if the box b overlaps with the current one
	bool Overlaps(const Box& b) const;

	//returns true if the point p is inside this box
	bool IsInside(const vec3& p) const;

	//returns  true if box b is inside this box
	bool IsInside(const Box& b) const;
	
	//creates a box which goes from [+infinity,- infinity] in al dimensions
	void Clear();
	
	//enlarges the box such that the point p is inside afterwards
	void Insert(const vec3& p);

	//enlarges the box such that box b is inside afterwards
	void Insert(const Box& b);

	//returns the point on or inside the box with the smallest distance to p 
	vec3 ClosestPoint(const vec3& p) const;

	//returns the squared distance between p and the box  
	float SqrDistance(const vec3& p) const;

	//returns the euclidean distance between p and the box 
	float Distance(const vec3& p) const;

};
