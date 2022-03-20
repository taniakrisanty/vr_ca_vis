// This source code is property of the Computer Graphics and Visualization 
// chair of the TU Dresden. Do not distribute! 
// Copyright (C) CGV TU Dresden - All Rights Reserved

#pragma once

#include <cgv/render/drawable.h>

typedef cgv::render::drawable::vec3 vec3;
typedef cgv::render::drawable::ivec3 ivec3;

//converts 3d floating point position pos into 3d integer grid cell index
inline ivec3 get_position_to_cell_index(const vec3& pos, const vec3& cell_extents) 
{
	ivec3 idx;
	for (int d = 0; d < 3; ++d)
	{
		idx[d] = int(pos[d] / cell_extents[d]);
			if (pos[d] < 0)
				--idx[d];
	}
	return idx;
}
