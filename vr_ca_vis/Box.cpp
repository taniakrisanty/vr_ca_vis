// This source code is property of the Computer Graphics and Visualization 
// chair of the TU Dresden. Do not distribute! 
// Copyright (C) CGV TU Dresden - All Rights Reserved

#include "Box.h"
#include "GridUtils.h"
#include <limits>


//creates an empty box like the method Clear
Box::Box()
{
	Clear();
}

//construct a box with the gven lower and upper corner points
Box::Box(const vec3& lbound, const vec3& ubound)
{
	bounds = std::make_pair(lbound,ubound);
}

//returns the corner point which is the lower bound of the box in all dimensions
vec3& Box::LowerBound()
{
	return bounds.first;
}

//returns the corner point which is the lower bound of the box in all dimensions
const vec3& Box::LowerBound() const
{
	return bounds.first;
}

//returns the corner point which is the upper bound of the box in all dimensions
vec3& Box::UpperBound()
{
	return bounds.second;
}

//returns the corner point which is the upper bound of the box in all dimensions
const vec3& Box::UpperBound() const
{
	return bounds.second;
}

//returns a vector containing the extents of the box in all dimensions
vec3 Box::Extents() const
{
	return UpperBound()-LowerBound();
}

//returns a vector containing the extents of the box in all dimensions divided by 2
vec3 Box::HalfExtents() const
{
	return Extents()/2.0f;
}
//returns the center of the box
vec3 Box::Center() const
{
	return (LowerBound()+UpperBound())/2.0f;
}

//returns the surface area of the box
float Box::SurfaceArea() const
{
	vec3 e = Extents();
	return 2.0f*(e[0]*e[1] + e[1]*e[2] + e[0]*e[2]);   
}

//returns the volume of the box
float Box::Volume() const
{
	vec3 e = Extents();
	return e[0] + e[1]+ e[2];
}

//returns the box radius for a given direction vector a 
//if the box is centered at the origin 
//then the projection of the box on direction a is contained within  the Interval [-r,r]
float Box::Radius(const vec3& a) const
{
	vec3 h = HalfExtents();
	return h[0]*std::abs(a[0]) + h[1]*std::abs(a[1]) + h[2]*std::abs(a[2]); 	
}

//returns true if the box b overlaps with the current one
bool Box:: Overlaps(const Box& b) const
{
	vec3 lb1 = LowerBound();
	vec3 ub1 = UpperBound();

	vec3 lb2 = b.LowerBound();
	vec3 ub2 = b.UpperBound();
		
	if(!OverlapIntervals(lb1[0], ub1[0],lb2[0], ub2[0]))
		return false;
	if(!OverlapIntervals(lb1[1], ub1[1],lb2[1], ub2[1]))
		return false;
	if(!OverlapIntervals(lb1[2], ub1[2],lb2[2], ub2[2]))
		return false;
		
	return true;
}

//returns true if the point p is inside this box
bool Box::IsInside(const vec3& p) const
{
	const vec3& lbound = LowerBound();
	const vec3& ubound = UpperBound();
	if(p[0] < lbound[0] || p[0] > ubound[0] ||
		p[1] < lbound[1] || p[1] > ubound[1] ||
		p[2] < lbound[2] || p[2] > ubound[2])
		return false;
	return true;		
}

//returns  true if box b is inside this box
bool Box::IsInside(const Box& b) const
{
	return IsInside(b.LowerBound()) && IsInside(b.UpperBound());
}

//creates a box which goes from [+infinity,- infinity] in al dimensions
void Box::Clear()
{
	const float infty = std::numeric_limits<float>::infinity();
	const vec3 vec_infty = vec3(infty,infty,infty);
	bounds = std::make_pair(vec_infty,-vec_infty);	
}
 
//enlarges the box such that the point p is inside afterwards
void Box::Insert(const vec3& p)
{
	vec3& lbound = LowerBound();
	vec3& ubound = UpperBound();
	lbound[0] = (std::min)(p[0],lbound[0]);
	lbound[1] = (std::min)(p[1],lbound[1]);
	lbound[2] = (std::min)(p[2],lbound[2]);
	ubound[0] = (std::max)(p[0],ubound[0]);
	ubound[1] = (std::max)(p[1],ubound[1]);
	ubound[2] = (std::max)(p[2],ubound[2]);
}

//enlarges the box such that box b is inside afterwards
void Box::Insert(const Box& b)
{
	Insert(b.LowerBound());
	Insert(b.UpperBound());
}

//returns the point on or inside the box with the smallest distance to p 
vec3 Box::ClosestPoint(const vec3& p) const
{
	vec3 q;
	const vec3& lbound = LowerBound();
	const vec3& ubound = UpperBound();
		
	q[0] = std::min(std::max(p[0],lbound[0]),ubound[0]);
	q[1] = std::min(std::max(p[1],lbound[1]),ubound[1]);
	q[2] = std::min(std::max(p[2],lbound[2]),ubound[2]);
			
	return q;
}

//returns the squared distance between p and the box  
float Box::SqrDistance(const vec3& p) const
{
	vec3 d = p-ClosestPoint(p);
	return dot(d, d);
}

//returns the euclidean distance between p and the box 
float Box::Distance(const vec3& p) const
{
	return sqrt(SqrDistance(p));
}


