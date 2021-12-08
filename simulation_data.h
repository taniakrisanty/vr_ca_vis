#pragma once

#include <cgv/render/render_types.h>

class cell : public cgv::render::render_types
{
public:
	typedef cgv::math::fvec<float, 3> svec3;
	//typedef cgv::math::fvec<int16_t, 3> svec3;

protected:
	uint16_t time; // time

	uint16_t id; // cell.id
	uint8_t type; // cell.type

	svec3 point; // l.x, l.y, l.z

	float attr; // b, b2

public:
	cell(uint16_t cell_id, int16_t x, int16_t y, int16_t z, float cell_attr) : time(0), id(cell_id), type(0), point(svec3(x, y, z)), attr(cell_attr)
	{}

	cell(uint16_t cell_time, uint16_t cell_id, uint8_t cell_type, int16_t x, int16_t y, int16_t z, float cell_attr) : time(cell_time), id(cell_id), type(cell_type), point(svec3(x, y, z)), attr(cell_attr)
	{}

	uint16_t get_time() const
	{
		return time;
	}

	uint16_t get_id() const
	{
		return id;
	}

	uint8_t get_type() const
	{
		return type;
	}

	svec3 get_point() const
	{
		return point;
	}

	float get_attr() const
	{
		return attr;
	}
};

class cell_vis : public cell
{
protected:
	rgba8 color;

public:
	cell_vis(uint16_t cell_id, int16_t x, int16_t y, int16_t z, float cell_attr) : cell(cell_id, x, y, z, cell_attr), color(rgba8())
	{}

	cell_vis(uint16_t cell_time, uint16_t cell_id, uint8_t cell_type, int16_t x, int16_t y, int16_t z, float cell_attr, rgba8 cell_color) : cell(cell_time, cell_id, cell_type, x, y, z, cell_attr), color(cell_color)
	{}

	rgba8 get_color() const
	{
		return color;
	}
};

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
