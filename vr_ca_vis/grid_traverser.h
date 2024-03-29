﻿#pragma once

#include <cgv/render/render_types.h>

class grid_traverser : cgv::render::render_types
{
	//ray origin and direction
	vec3 origin, direction;
	//grid cell extents
	vec3 cell_extents;
	//current cell index
	ivec3 current;

	double step_x, step_y, step_z;
	double t_max_x, t_max_y, t_max_z;
	double t_delta_x, t_delta_y, t_delta_z;

public:
	//constructs a grid traverser for a given ray with origin o, and ray direction d for a grid with cell extents ce
	grid_traverser(const vec3& o, const vec3&d, const vec3& ce);

	//accessor of ray origin
	vec3& get_origin();

	//const accessor of ray origin
	const vec3& get_origin() const;

	//accessor of ray direction
	vec3& get_direction();
	
	//const accessor of ray direction
	const vec3& get_direction() const;	

	//set cell extents
	void set_cell_extents(const vec3& cell_extent);	

	//init at origin cell
	void init();

	//step to next cell along ray direction
	void operator++(int);

	//return current cell index
	ivec3 operator*();		
};
