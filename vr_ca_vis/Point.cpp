// This source code is property of the Computer Graphics and Visualization 
// chair of the TU Dresden. Do not distribute! 
// Copyright (C) CGV TU Dresden - All Rights Reserved

#include "Point.h"
#include "GridUtils.h"

//default constructor
Point::Point(){}
	
//construct a point with given point position v0
Point::Point(const vec3& v0) : v0(v0)
{
}
		
//returns axis aligned bounding box of point
Box Point::ComputeBounds() const
{
	Box b;	
	b.Insert(v0);
		
	return b;
}

//returns true if point overlap with box b
bool Point::Overlaps(const Box& b) const
{
	vec3 lb = b.LowerBound();
	vec3 ub = b.UpperBound();
	return 
		(v0[0] >= lb[0] && v0[0] <= ub[0]&&
		v0[1] >= lb[1] && v0[1] <= ub[1] &&
		v0[2] >= lb[2] && v0[2] <= ub[2]);		   
}

//returns the point position
vec3 Point::ClosestPoint(const vec3& p) const
{
	return v0;
}

//returns the squared distance between the query point p and the current point
float Point::SqrDistance(const vec3& p) const
{
	vec3 d = p-ClosestPoint(p);
	return d.sqr_length();
}

//returns the euclidean distance between the query point p and the current point
float Point::Distance(const vec3& p) const
{
	return sqrt(SqrDistance(p));
}

//returns a the position of the point as a reference point which is used to sort the primitive in the AABB tree construction
vec3 Point::ReferencePoint() const
{
	return v0;
}
