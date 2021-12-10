// This source code is property of the Computer Graphics and Visualization 
// chair of the TU Dresden. Do not distribute! 
// Copyright (C) CGV TU Dresden - All Rights Reserved

#pragma once

#include <cgv/render/drawable.h>

#include "Box.h"

typedef cgv::render::drawable::vec3 vec3;
typedef cgv::render::drawable::ivec3 ivec3;

class Point
{
	//internal storage of point position
	vec3 v0;

public:
	
	//default constructor
	Point();
	
	//construct a point with given point position v0
	Point(const vec3& v0);
		
	//returns axis aligned bounding box of point
	Box ComputeBounds() const;

	//returns true if point overlap with box b
	bool Overlaps(const Box& b) const;

	//returns the point position
	vec3 ClosestPoint(const vec3& p) const;

	//returns the squared distance between the query point p and the current point
	float SqrDistance(const vec3& p) const;

	//returns the euclidean distance between the query point p and the current point
	float Distance(const vec3& p) const;

	//returns a the position of the point as a reference point which is used to sort the primitive in the AABB tree construction
	vec3 ReferencePoint() const;
};
