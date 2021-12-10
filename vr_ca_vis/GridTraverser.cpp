// This source code is property of the Computer Graphics and Visualization 
// chair of the TU Dresden. Do not distribute! 
// Copyright (C) CGV TU Dresden - All Rights Reserved

#include "GridTraverser.h"
#include "GridUtils.h"


GridTraverser::GridTraverser()
{ }

GridTraverser::GridTraverser(const vec3& o, const vec3&d, const vec3& cell_extents)
	: orig(o), dir(d), cellExtents(cell_extents)
{
	dir.normalize();
	Init();
}

vec3& GridTraverser::Origin()
{
	return orig;
}
const vec3& GridTraverser::Origin() const
{
	return orig;
}

vec3& GridTraverser::Direction()
{
	return dir;
}

const vec3& GridTraverser::Direction() const
{
	return dir;
}

void GridTraverser::SetCellExtents(const vec3& cellExtent)
{
	this->cellExtents = cellExtent;
	Init();
}

void GridTraverser::Init()
{
	current = PositionToCellIndex(orig, cellExtents);
	//<snippet task="4.2.2">
	//<student>
	///* Task 4.2.2 */
	////you can add some precalculation code here
	//</student>
	//<solution>
	for (int d = 0; d < 3; ++d)
	{

		step[d] = dir[d] < 0 ? -1 : (dir[d] > 0 ? 1 : 0);

		if (dir[d] == 0)
		{
			tmax[d] = std::numeric_limits<float>::infinity();
			tdelta[d] = std::numeric_limits<float>::infinity();
			tmin[d] = -std::numeric_limits<float>::infinity();

		}
		else
		{

			float t1 = (current[d] * cellExtents[d] - orig[d]) / dir[d];
			float t2 = ((current[d] + 1)*cellExtents[d] - orig[d]) / dir[d];
			if (t1 > t2)
			{
				tmax[d] = t1;
				tmin[d] = t2;
				tdelta[d] = t1 - t2;
			}
			else
			{
				tmax[d] = t2;
				tmin[d] = t1;
				tdelta[d] = t2 - t1;
			}
		}
	}
	//</solution>
	//</snippet>
}

void GridTraverser::operator++(int)
{
	//<snippet task="4.2.2">
	//<student>
	///* Task 4.2.2 */
	////traverse one step along the ray
	////update the cell index stored in attribute "current"
	//</student>
	//<solution>
	int i = 0;
	float tbest = tmax[0];
	if (tmax[1] < tbest)
	{
		i = 1;
		tbest = tmax[1];
	}
	if (tmax[2] < tbest)
	{
		i = 2;
		tbest = tmax[2];
	}
	current[i] += step[i];
	tmax[i] += tdelta[i];
	//</solution>
	//</snippet>
}

ivec3 GridTraverser::operator*()
{
	return current;
}

	
