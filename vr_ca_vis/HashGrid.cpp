// This source code is property of the Computer Graphics and Visualization 
// chair of the TU Dresden. Do not distribute! 
// Copyright (C) CGV TU Dresden - All Rights Reserved

#include "HashGrid.h"
#include <iostream>

void BuildHashGridFromVertices(const std::vector<ivec3>& points, HashGrid<Point>& grid, const vec3& cellSize)
{
	std::cout << "Building hash grid from vertices .." << std::endl;
	grid = HashGrid<Point>(cellSize, 1);
	for (auto point : points)
	{
		grid.Insert(Point(point));
	}

	std::cout << "Done (using " << grid.NumCells() << " cells)." << std::endl;
}
