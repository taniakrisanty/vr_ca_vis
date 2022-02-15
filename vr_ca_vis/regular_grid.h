// This source code is property of the Computer Graphics and Visualization 
// chair of the TU Dresden. Do not distribute! 
// Copyright (C) CGV TU Dresden - All Rights Reserved

#pragma once

#include <queue>

#include "grid_utils.h"

#include <cgv/render/render_types.h>

typedef cgv::math::fvec<float, 3> vec3;

template <typename T>
class regular_grid
{
public:
	//result entry for nearest and k nearest primitive queries
	struct result_entry
	{
		//squared distance from query point to primitive
		float sqr_distance;
		//primitive index
		int prim;
		//default constructor
		result_entry()
			: sqr_distance(std::numeric_limits<float>::infinity()), prim(-1)
		{ }
		//constructor
		result_entry(float _sqr_distance, int _prim)
			: sqr_distance(_sqr_distance), prim(_prim)
		{ }
		//result_entry are sorted by their sqr_distance using this less than operator 
		bool operator<(const result_entry& e) const
		{
			return sqr_distance < e.sqr_distance;
		}
	};

private:
	std::vector<size_t>* grid = NULL;

	bool* visited_statuses = NULL;

	vec3 cell_extents;

	const std::vector<T>* cells;

	//search entry used internally for nearest and k nearest primitive queries
	struct search_entry
	{
		//squared distance to node from query point
		float sqr_distance;
		//node
		int grid_index;

		//constructor
		search_entry(float _sqr_distance, int _grid_index)
			: sqr_distance(_sqr_distance), grid_index(_grid_index)
		{ }

		//search entry a < b means a.sqr_distance > b. sqr_distance 
		bool operator<(const search_entry& e) const
		{
			return sqr_distance > e.sqr_distance;
		}
	};

	struct knn_result
	{
		size_t k;

		std::priority_queue<result_entry> queue;

		//default constructor
		knn_result()
			: k(0)
		{ }
		//constructor
		knn_result(size_t k)
			: k(k)
		{ }
		// 
		float max_dist() const
		{
			return k > 0 && queue.size() < k ? std::numeric_limits<float>::infinity() : queue.top().sqr_distance;
		}
		//
		void consider(int prim, float sqr_distance)
		{
			if (k > 0 && queue.size() == k)
			{
				if (sqr_distance >= queue.top().sqr_distance)
					return;

				queue.pop();
			}
			queue.push(result_entry(sqr_distance, prim));
		}
	};

public:
	regular_grid(const float cellExtent = 10)
	{
		cell_extents[0] = cell_extents[1] = cell_extents[2] = cellExtent;
	}

	void clear()
	{
		if (grid) delete[] grid;
		if (visited_statuses) delete[] visited_statuses;

		// TODO divide the space by the cell_extents
		grid = new std::vector<size_t>[10 * 10 * 10];
		visited_statuses = new bool[10 * 10 * 10];
	}

	int get_cell_index_to_grid_index(const ivec3& ci) const
	{
		return ci.x() + ci.y() * 10 + ci.z() * 10 * 10;
	}

	//converts a position to a grid index
	int get_position_to_grid_index(const vec3& pos) const
	{
		vec3 ci = get_position_to_cell_index(pos, cell_extents);

		return get_cell_index_to_grid_index(ci);
	}

	//return the center position of a cell specified by its cell key
	vec3 get_cell_center(const ivec3& idx) const
	{
		vec3 p;
		for (int d = 0; d < 3; ++d)
			p[d] = (idx[d] + 0.5f) * cell_extents[d];

		return p;
	}

	//return the center position of a cell containing give position pos
	vec3 get_cell_center(const vec3& pos) const
	{
		return get_cell_center(get_position_to_cell_index(pos, cell_extents));
	}

	void get_closest_indices(int index, std::vector<int>& indices) const
	{
		if (grid == NULL || index < 0 || index >= 10 * 10 * 10)
			return;

		indices.assign(grid[index].begin(), grid[index].end());
	}

	void get_closest_indices(const vec3& pos, std::vector<int>& indices) const
	{
		int index = get_position_to_grid_index(pos);

		get_closest_indices(index, indices);
	}

	//returns the extents of a grid cell
	vec3 get_cell_extents() const
	{
		return cell_extents;
	}

	//inserts primitive p into all overlapping regular grid cells
	void insert(int p_index, const vec3& p)
	{
		int index = get_position_to_grid_index(p);

		grid[index].push_back(p_index);
	}

	void consider_path(const ivec3& ci, const vec3& q, std::priority_queue<search_entry>& qmin, knn_result& res) const
	{
		int gi = get_cell_index_to_grid_index(ci);

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
				gi = get_cell_index_to_grid_index(cell_index);

				// ignore if already inside the queue
				if (visited_statuses[gi])
					continue;

				// ignore if outside the grid
				if (gi < 0 || gi >= 10 * 10 * 10)
					continue;

				qmin.push(search_entry((get_cell_center(cell_index) - q).sqr_length(), gi));
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
				res.consider(p_index, (cells->at(p_index).node - q).sqr_length());
			}
		}
	}

	result_entry closest_point(const vec3& q) const
	{
		if (grid != NULL && visited_statuses != NULL)
		{
			memset(visited_statuses, false, sizeof(bool) * 10 * 10 * 10);

			ivec3 ci = get_position_to_cell_index(q, cell_extents);

			knn_result k_best(1);

			std::priority_queue<search_entry> qmin;
			consider_path(ci, q, qmin, k_best);

			while (!qmin.empty())
			{
				search_entry se = qmin.top();
				int gi = se.grid_index;
				float dist = se.sqr_distance;
				qmin.pop();

				if (dist > k_best.max_dist())
					break;

				consider_path(gi, q, qmin, k_best);
			}

			if (!k_best.queue.empty())
				return k_best.queue.top();
		}

		return result_entry();
	}

	void print() const
	{
		for (int i = 0; i < 10; ++i)
		{
			for (int j = 0; j < 10; ++j)
			{
				for (int k = 0; k < 10; ++k)
				{
					int gi = get_cell_index_to_grid_index(ivec3(i, j, k));

					std::cout << "Grid[" << i << ", " << j << ", " << k << "]" << std::endl;

					for (size_t index : grid[gi])
					{
						std::cout << cells->at(index).node << std::endl;
					}

					std::cout << "===============" << std::endl;
				}
			}
		}
	}

	void build_from_vertices(const std::vector<T>* _cells)
	{
		clear();
		
		cells = _cells;

		if (cells == NULL)
			return;

		size_t index = 0;
		for (std::vector<T>::const_iterator vit = cells->begin(), vend = cells->end(); vit != vend; ++vit)
			insert(index++, vit->node);
	}
};
