// This source code is property of the Computer Graphics and Visualization 
// chair of the TU Dresden. Do not distribute! 
// Copyright (C) CGV TU Dresden - All Rights Reserved

#pragma once

#include <array>

#include <cgv/render/render_types.h>

typedef cgv::math::fvec<float, 3> vec3;
typedef cgv::math::fvec<int, 3> ivec3;

class grid_traverser
{
	//ray origin and direction
	vec3 orig, dir;
	//grid cell extents
	vec3 cellExtents;
	//current cell index
	ivec3 current;

	/* you can additional attributes for incremental calculation here */
	double stepX, stepY, stepZ;
	double tMaxX, tMaxY, tMaxZ;
	double tDeltaX, tDeltaY, tDeltaZ;

public:
	//default constructor
	grid_traverser();

	//constructs a grid traverser for a given ray with origin o, and ray direction d for a grid with cell extents ce
	grid_traverser(const vec3& o, const vec3&d, const vec3& ce);

	//accessor of ray origin
	vec3& Origin();

	//const accessor of ray origin
	const vec3& Origin() const;

	//accessor of ray direction
	vec3& Direction();
	
	//const accessor of ray direction
	const vec3& Direction() const;	

	//set cell extents
	void SetCellExtents(const vec3& cellExtent);	

	//init at origin cell
	void Init();

	//step to next cell along ray direction
	void operator++(int);

	//return current cell index
	ivec3 operator*();		
};
