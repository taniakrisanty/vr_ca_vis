#pragma once

#include <cgv/render/drawable.h>

struct simulation_data
{
	typedef cgv::render::drawable::vec2 vec2;
	typedef cgv::render::drawable::vec3 vec3;

	std::vector<int> ids;
	std::vector<int> types;
	std::vector<vec3> points;

	std::vector<float> times;
	std::vector<uint64_t> time_step_starts;
};
