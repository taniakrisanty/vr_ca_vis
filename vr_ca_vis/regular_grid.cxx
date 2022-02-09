#include "regular_grid.h"
#include <iostream>

void BuildRegularGridFromVertices(const std::vector<vec3>& points, regular_grid& grid)
{
	//grid = regular_grid();
	grid.setPoints(points);

	int i = 0;
	auto vend = points.end();
	for (auto vit = points.begin(); vit != vend; ++vit)
		grid.Insert(i++, *vit);
}
