#include "regular_grid.h"
#include <iostream>

void BuildRegularGridFromVertices(const std::vector<vec3>& points, regular_grid& grid)
{
	std::cout << "Building regular grid from vertices .." << std::endl;
	//grid = regular_grid();
	grid.clear();
	grid.setPoints(points);

	int i = 0;
	auto vend = points.end();
	for (auto vit = points.begin(); vit != vend; ++vit)
		grid.Insert(i++, *vit);
}
