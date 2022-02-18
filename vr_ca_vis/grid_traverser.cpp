// This source code is property of the Computer Graphics and Visualization 
// chair of the TU Dresden. Do not distribute! 
// Copyright (C) CGV TU Dresden - All Rights Reserved

#include "grid_traverser.h"
#include "grid_utils.h"
#include <cfloat>

grid_traverser::grid_traverser()
{ }

grid_traverser::grid_traverser(const vec3& _origin, const vec3& _direction, const vec3& _cell_extents)
	: origin(_origin), direction(_direction), cell_extents(_cell_extents)
{
	direction.normalize();
	init();
}

vec3& grid_traverser::get_origin()
{
	return origin;
}
const vec3& grid_traverser::get_origin() const
{
	return origin;
}

vec3& grid_traverser::get_direction()
{
	return direction;
}

const vec3& grid_traverser::get_direction() const
{
	return direction;
}

void grid_traverser::set_cell_extents(const vec3& _cell_extents)
{
	cell_extents = _cell_extents;
	init();
}

void grid_traverser::init()
{
	current = get_position_to_cell_index(origin, cell_extents);
		
	// step_x, step_y, and step_z are initialized to either 1 or -1
	// indicating whether current (x, y, and z) is incremented or decremented as the ray crosses voxel boundaries
	step_x = direction[0] > 0 ? 1 : direction[0] < 0 ? -1 : 0;
	step_y = direction[1] > 0 ? 1 : direction[1] < 0 ? -1 : 0;
	step_z = direction[2] > 0 ? 1 : direction[2] < 0 ? -1 : 0;

	// next_x, next_y, and next_z are set to next voxel
	double next_x = (current[0] + step_x) * cell_extents[0];
  	double next_y = (current[1] + step_y) * cell_extents[1];
  	double next_z = (current[2] + step_z) * cell_extents[2];

	// t_max_x, t_max_y, and t_max_z are set to the distance until next intersection with voxel border
	t_max_x = (direction[0] != 0) ? (next_x - origin[0]) / direction[0] : DBL_MAX;
  	t_max_y = (direction[1] != 0) ? (next_y - origin[1]) / direction[1] : DBL_MAX;
  	t_max_z = (direction[2] != 0) ? (next_z - origin[2]) / direction[2] : DBL_MAX;

	// t_delta_x, t_delta_y, t_delta_z are set to how far along the ray we must move (in units of t)
	// in the direction of x-, y-, and z-axis to equal the width, height, and depth of a voxel, respectively
	t_delta_x = (direction[0] != 0) ? cell_extents[0] / direction[0] * step_x : DBL_MAX;
  	t_delta_y = (direction[1] != 0) ? cell_extents[1] / direction[1] * step_y : DBL_MAX;
  	t_delta_z = (direction[2] != 0) ? cell_extents[2] / direction[2] * step_z : DBL_MAX;
}

void grid_traverser::operator++(int)
{
	// traverse one step along the ray
	// update the cell index stored in attribute "current"

    if (t_max_x < t_max_y)
	{
		if (t_max_x < t_max_z)
		{
        	current[0] += step_x;
        	t_max_x += t_delta_x;
      	}
		else
		{
        	current[2] += step_z;
        	t_max_z += t_delta_z;
		}
    }
	else
	{
      	if (t_max_y < t_max_z)
		{
        	current[1] += step_y;
        	t_max_y += t_delta_y;
      	}
		else
		{
        	current[2] += step_z;
        	t_max_z += t_delta_z;
      	}
    }
}
ivec3 grid_traverser::operator*()
{
	return current;
}
