#pragma once

#include <cgv/render/render_types.h>

struct simulation_data : public cgv::render::render_types // change into interleaved data, check previous implementation
{
	// additional attributes are floats
	/// declare type of 3d integer vectors
	typedef cgv::math::fvec<int16_t, 3> svec3;

	std::vector<int> ids; // cell_id change int into short or int16
	std::vector<int> types; // store type per id, change into int16
	std::vector<vec3> points; // change float into short, check in GLSL

	std::vector<float> times;
	std::vector<uint64_t> time_step_starts;

	// visibility test on shader pipeline or on application

	//std::vector<cell_vis> cells;
};
// add prefix in variable names to differentiate between cell or ... variable
// getter and setter function
// vis data (not simulation data) color attr per cell id, start with random colors
//lineage tree
//life span

// todo: clarify export problem, binary files (cae file format, custom time frame), color
// intersection bb then voxel

// filtering: clipping planes
