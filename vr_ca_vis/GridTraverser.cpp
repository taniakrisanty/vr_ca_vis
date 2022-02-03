// This source code is property of the Computer Graphics and Visualization 
// chair of the TU Dresden. Do not distribute! 
// Copyright (C) CGV TU Dresden - All Rights Reserved

#include "GridTraverser.h"
#include "GridUtils.h"
#include <cfloat>

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
	
	/* Task 4.2.2 */
	
	// stepX, stepY, and stepZ are initialized to either 1 or -1
	// indicating whether current (X, Y, and Z) is incremented or decremented as the ray crosses voxel boundaries
	stepX = dir[0] > 0 ? 1 : dir[0] < 0 ? -1 : 0;
	stepY = dir[1] > 0 ? 1 : dir[1] < 0 ? -1 : 0;
	stepZ = dir[2] > 0 ? 1 : dir[2] < 0 ? -1 : 0;

	// nextX, nextY, and nextZ are set to next voxel
	double nextX = (current[0] + stepX) * cellExtents[0];
  	double nextY = (current[1] + stepY) * cellExtents[1];
  	double nextZ = (current[2] + stepZ) * cellExtents[2];

	// tMaxX, tMaxY, and tMaxZ are set to the distance until next intersection with voxel border
	tMaxX = (dir[0] != 0) ? (nextX - orig[0]) / dir[0] : DBL_MAX;
  	tMaxY = (dir[1] != 0) ? (nextY - orig[1]) / dir[1] : DBL_MAX;
  	tMaxZ = (dir[2] != 0) ? (nextZ - orig[2]) / dir[2] : DBL_MAX;

	// tDeltaX, tDeltaY, tDeltaZ are set to how far along the ray we must move (in units of t)
	// in the direction of x-, y-, and z-axis to equal the width, height, and depth of a voxel, respectively.
	tDeltaX = (dir[0] != 0) ? cellExtents[0] / dir[0] * stepX : DBL_MAX;
  	tDeltaY = (dir[1] != 0) ? cellExtents[1] / dir[1] * stepY : DBL_MAX;
  	tDeltaZ = (dir[2] != 0) ? cellExtents[2] / dir[2] * stepZ : DBL_MAX;
}

void GridTraverser::operator++(int)
{
	/* Task 4.2.2 */

	//traverse one step along the ray
	//update the cell index stored in attribute "current"

    if (tMaxX < tMaxY)
	{
		if (tMaxX < tMaxZ)
		{
        	current[0] += stepX;
        	tMaxX += tDeltaX;
      	}
		else
		{
        	current[2] += stepZ;
        	tMaxZ += tDeltaZ;
		}
    }
	else
	{
      	if (tMaxY < tMaxZ)
		{
        	current[1] += stepY;
        	tMaxY += tDeltaY;
      	}
		else
		{
        	current[2] += stepZ;
        	tMaxZ += tDeltaZ;
      	}
    }
}
ivec3 GridTraverser::operator*()
{
	return current;
}

	
