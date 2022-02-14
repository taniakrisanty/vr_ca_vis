// This source code is property of the Computer Graphics and Visualization 
// chair of the TU Dresden. Do not distribute! 
// Copyright (C) CGV TU Dresden - All Rights Reserved

#pragma once

#include <queue>

#include "grid_utils.h"

#include <cgv/render/render_types.h>

typedef cgv::math::fvec<float, 3> vec3;

class regular_grid
{
public:
	//result entry for nearest and k nearest primitive queries
	struct ResultEntry
	{
		//squared distance from query point to primitive
		float sqrDistance;
		//primitive index
		int prim;
		//default constructor
		ResultEntry()
			: sqrDistance(std::numeric_limits<float>::infinity()), prim(-1)
		{ }
		//constructor
		ResultEntry(float sqrDistance, int p)
			: sqrDistance(sqrDistance), prim(p)
		{ }
		//result_entry are sorted by their sqr_distance using this less than operator 
		bool operator<(const ResultEntry& e) const
		{
			return sqrDistance < e.sqrDistance;
		}
	};

private:
	std::vector<int>* grid = NULL;

	bool* visited_statuses = NULL;

	vec3 cellExtents;

	std::vector<vec3> points;

	//search entry used internally for nearest and k nearest primitive queries
	struct SearchEntry
	{
		//squared distance to node from query point
		float sqrDistance;
		//node
		int gridIndex;

		//constructor
		SearchEntry(float sqrDistance, int gridIndex)
			: sqrDistance(sqrDistance), gridIndex(gridIndex)
		{ }

		//search entry a < b means a.sqr_distance > b. sqr_distance 
		bool operator<(const SearchEntry& e) const
		{
			return sqrDistance > e.sqrDistance;
		}
	};

	struct KnnResult
	{
		size_t k;

		std::priority_queue<ResultEntry> queue;

		//default constructor
		KnnResult()
			: k(0)
		{ }
		//constructor
		KnnResult(size_t k)
			: k(k)
		{ }
		// 
		float maxDist() const
		{
			return k > 0 && queue.size() < k ? std::numeric_limits<float>::infinity() : queue.top().sqrDistance;
		}
		//
		void consider(int prim, float sqrDistance)
		{
			if (k > 0 && queue.size() == k)
			{
				if (sqrDistance >= queue.top().sqrDistance)
					return;

				queue.pop();
			}
			queue.push(ResultEntry(sqrDistance, prim));
		}
	};

public:
	regular_grid(const float cellExtent = 10)
	{
		cellExtents[0] = cellExtents[1] = cellExtents[2] = cellExtent;
	}

	void clear()
	{
		if (grid) delete[] grid;
		if (visited_statuses) delete[] visited_statuses;

		// TODO divide the space by the cellExtent
		grid = new std::vector<int>[10 * 10 * 10];
		visited_statuses = new bool[10 * 10 * 10];

		points.clear();
	}

	int CellIndexToGridIndex(const ivec3& ci) const
	{
		return ci.x() + ci.y() * 10 + ci.z() * 10 * 10;
	}

	//converts a position to a grid index
	int PositionToGridIndex(const vec3& pos) const
	{
		vec3 ci = PositionToCellIndex(pos, cellExtents);

		return CellIndexToGridIndex(ci);
	}

	//return the center position of a cell specified by its cell key
	vec3 CellCenter(const ivec3& idx) const
	{
		vec3 p;
		for (int d = 0; d < 3; ++d)
			p[d] = (idx[d] + 0.5f) * cellExtents[d];

		return p;
	}

	//return the center position of a cell containing give position pos
	vec3 CellCenter(const vec3& pos) const
	{
		return CellCenter(PositionToCellIndex(pos, cellExtents));
	}

	void ClosestIndices(int index, std::vector<int>& indices) const
	{
		if (grid == NULL || index < 0 || index >= 10 * 10 * 10)
			return;

		indices.assign(grid[index].begin(), grid[index].end());
	}

	void ClosestIndices(const vec3& pos, std::vector<int>& indices) const
	{
		int index = PositionToGridIndex(pos);

		ClosestIndices(index, indices);
	}

	//returns the extents of a grid cell
	vec3 CellExtents() const
	{
		return cellExtents;
	}

	//inserts primitive p into all overlapping regular grid cells
	void Insert(int p_index, const vec3& p)
	{
		int index = PositionToGridIndex(p);

		grid[index].push_back(p_index);

		points.push_back(p);
	}

	void considerPath(const ivec3& ci, const vec3& q, std::priority_queue<SearchEntry>& qmin, KnnResult& res) const
	{
		int gi = CellIndexToGridIndex(ci);

		// ignore if outside the grid
		if (gi < 0 || gi >= 10 * 10 * 10)
			return;

		if (grid[gi].empty())
		{
			std::vector<ivec3> cis{
				ivec3(ci.x() - 1, ci.y(), ci.z()),	// left
				ivec3(ci.x() + 1, ci.y(), ci.z()),	// right
				ivec3(ci.x(), ci.y() - 1, ci.z()),	// bottom
				ivec3(ci.x(), ci.y() + 1, ci.z()),	// top
				ivec3(ci.x(), ci.y(), ci.z() - 1),	// back
				ivec3(ci.x(), ci.y(), ci.z() + 1)	// front
			};

			for (const ivec3& cell_index : cis)
			{
				gi = CellIndexToGridIndex(cell_index);

				// ignore if already inside the queue
				if (visited_statuses[gi])
					continue;

				// ignore if outside the grid
				if (gi < 0 || gi >= 10 * 10 * 10)
					continue;

				qmin.push(SearchEntry((CellCenter(cell_index) - q).sqr_length(), gi));
			}
		}
		else
		{
			// ignore if already inside the queue
			if (visited_statuses[gi])
				return;

			visited_statuses[gi] = true;

			for (int p_index : grid[gi])
			{
				res.consider(p_index, (points[p_index] - q).sqr_length());
			}
		}
	}

	ResultEntry ClosestPoint(const vec3& q) const
	{
		if (grid != NULL && visited_statuses != NULL)
		{
			memset(visited_statuses, false, sizeof(bool) * 10 * 10 * 10);

			ivec3 ci = PositionToCellIndex(q, cellExtents);

			KnnResult k_best(1);

			std::priority_queue<SearchEntry> qmin;
			considerPath(ci, q, qmin, k_best);

			while (!qmin.empty())
			{
				SearchEntry se = qmin.top();
				int gi = se.gridIndex;
				float dist = se.sqrDistance;
				qmin.pop();

				if (dist > k_best.maxDist())
					break;

				considerPath(gi, q, qmin, k_best);
			}

			if (!k_best.queue.empty())
				return k_best.queue.top();
		}

		return ResultEntry();
	}

	void Debug() const
	{
		for (int i = 0; i < 10; ++i)
		{
			for (int j = 0; j < 10; ++j)
			{
				for (int k = 0; k < 10; ++k)
				{
					int gi = CellIndexToGridIndex(ivec3(i, j, k));

					std::cout << "Grid[" << i << ", " << j << ", " << k << "]" << std::endl;

					for (int index : grid[gi])
					{
						std::cout << points[index] << std::endl;
					}

					std::cout << "===============" << std::endl;
				}
			}
		}
	}
};

//helper function to construct a regular grid data structure from the centers of the cells
template <typename T>
void BuildRegularGridFromVertices(const std::vector<T>& cells, regular_grid& grid)
{
	grid.clear();

	int i = 0;
	auto vend = cells.end();
	for (auto vit = cells.begin(); vit != vend; ++vit)
		grid.Insert(i++, vit->node);
}
