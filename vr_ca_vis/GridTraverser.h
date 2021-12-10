// This source code is property of the Computer Graphics and Visualization 
// chair of the TU Dresden. Do not distribute! 
// Copyright (C) CGV TU Dresden - All Rights Reserved

#pragma once

#include <array>

#include <cgv/render/drawable.h>

typedef cgv::render::drawable::vec3 vec3;
typedef cgv::render::drawable::ivec3 ivec3;

class GridTraverser
{
	//ray origin and direction
	vec3 orig,dir;
	//grid cell extents
	vec3 cellExtents;
	//current cell index
	ivec3 current;

	//<snippet task="4.2.2">
	//<student>
	///* you can additional attributes for incremental calculation here */
	//</student>
	//<solution>
	//integer step directions -1,0 or 1 for each dimension x,y,z
	ivec3 step;

	vec3 tmax, tmin, tdelta;
	//</solution>
	//</snippet>

public:
	//default constructor
	GridTraverser();

	//constructs a grid traverser for a given ray with origin o, and ray direction d for a grid with cell extents ce
	GridTraverser(const vec3& o, const vec3& d, const vec3& ce);

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
