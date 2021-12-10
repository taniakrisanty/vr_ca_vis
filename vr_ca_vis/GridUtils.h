// This source code is property of the Computer Graphics and Visualization 
// chair of the TU Dresden. Do not distribute! 
// Copyright (C) CGV TU Dresden - All Rights Reserved

#pragma once

#include <array>

#include <cgv/render/drawable.h>

typedef cgv::render::drawable::ivec3 ivec3;
typedef cgv::render::drawable::vec3 vec3;

//converts 3d floating point position pos into 3d integer grid cell index
inline ivec3 PositionToCellIndex(const vec3& pos, const vec3& cellExtents) 
{
	ivec3 idx;
	for(int d = 0; d < 3; ++d)
	{
		idx[d] = int(pos[d]/cellExtents[d]);
			if (pos[d] < 0)
				--idx[d];
	}
	return idx;	
}

//returns true if the two Interval [lb1,ub2] and [lb2,ub2] overlap 
inline bool OverlapIntervals(float lb1, float ub1, float lb2, float ub2)
{
	if(lb1 > ub1) 
			std::swap(lb1,ub1);
	if(lb2 > ub2) 
			std::swap(lb2,ub2);
	if(ub1 < lb2|| lb1 >ub2)
		return false;
	return true;	
}

