// This source code is property of the Computer Graphics and Visualization 
// chair of the TU Dresden. Do not distribute! 
// Copyright (C) CGV TU Dresden - All Rights Reserved

#pragma once


#include <chrono>
#include <mutex>
#include <queue>
#include <thread>

#include "grid_utils.h"

#include <cgv/render/drawable.h>

template <typename T>
class regular_grid : cgv::render::render_types
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
	mutable std::mutex mutex;
	std::thread thread;

	bool build_grid = false;

	// TODO change into size_t* if nodes do not overlap
	//std::vector<size_t>* grid = NULL;
	size_t* grid = NULL;

	bool* visited_statuses = NULL;

	vec3 extents;
	vec3 cell_extents;

	const std::vector<T>* cells = NULL;
	size_t current_cell_index, cells_end;

	const std::vector<vec4>* clipping_planes = NULL;

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

	void build_from_vertices_impl(bool print_grid = false)
	{
		auto start = std::chrono::high_resolution_clock::now();

		while (true)
		{
			{
				std::lock_guard<std::mutex> lock(mutex);

				if (!build_grid)
				{
					std::cout << "build_from_vertices cancelled at index " << current_cell_index << std::endl;
					break;
				}

				if (current_cell_index == cells_end)
				{
					auto stop = std::chrono::high_resolution_clock::now();

					auto duration = std::chrono::duration_cast<std::chrono::microseconds>(stop - start);

					std::cout << "Time taken by multithreaded build_from_vertices: " << duration.count() << " microseconds" << std::endl;

					build_grid = false;

					if (print_grid) print();
					break;
				}

				insert(current_cell_index, cells->at(current_cell_index).node);

				++current_cell_index;
			}

			std::this_thread::yield();
		}
	}

public:
	regular_grid(const float _cell_extents = 1.f) : current_cell_index(0), cells_end(0)
	{
		cell_extents[0] = cell_extents[1] = cell_extents[2] = _cell_extents;
	}

	void reset(const ivec3& _extents)
	{
		vec3 e(_extents[0] / cell_extents[0], _extents[1] / cell_extents[1], _extents[2] / cell_extents[2]);

		if (e != extents)
		{
			if (grid) delete[] grid;
			if (visited_statuses) delete[] visited_statuses;

			extents[0] = _extents[0] / cell_extents[0];
			extents[1] = _extents[1] / cell_extents[1];
			extents[2] = _extents[2] / cell_extents[2];

			//grid = new std::vector<size_t>[extents.x() * extents.y() * extents.z()];
			grid = new size_t[extents.x() * extents.y() * extents.z()];
			visited_statuses = new bool[extents.x() * extents.y() * extents.z()];
		}

		memset(grid, 0, sizeof(size_t) * extents.x() * extents.y() * extents.z());
		memset(visited_statuses, false, sizeof(bool) * extents.x() * extents.y() * extents.z());
	}

	int get_cell_index_to_grid_index(const ivec3& ci) const
	{
		if (ci.x() < 0 || ci.x() >= extents.x() ||
			ci.y() < 0 || ci.y() >= extents.y() ||
			ci.z() < 0 || ci.z() >= extents.z())
			return -1;

		// TODO check
		return ci.x() + ci.y() * extents.x() + ci.z() * extents.y() * extents.z();
	}

	//converts a position to a grid index
	int get_position_to_grid_index(const vec3& pos) const
	{
		ivec3 ci = get_position_to_cell_index(pos, cell_extents);

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
		if (index < 0 || index >= extents.x() * extents.y() * extents.z()) // grid cell index is out of bounds
			return;

		std::lock_guard<std::mutex> lock(mutex);

		if (!build_grid && grid != NULL)
		{
			//for (size_t p_index : grid[index])
			size_t p_index = grid[index];
			if (p_index == 0) // grid cell is empty
				return;

			p_index -= 1;

			vec4 node = cells->at(p_index).node.lift();

			bool clipped = false;

			// ignore if cell is invisible (clipped by the clipping plane)
			for (const vec4& cp : *clipping_planes)
			{
				if (dot(node, cp) < 0)
				{
					clipped = true;
					break;
				}
			}

			if (!clipped)
				indices.push_back(p_index);
		}
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

		//grid[index].push_back(p_index);
		grid[index] = p_index + 1;
	}

	void consider_path(const ivec3& ci, const vec3& q, std::priority_queue<search_entry>& qmin, knn_result& res) const
	{
		int gi = get_cell_index_to_grid_index(ci);

		// ignore if outside the grid
		//if (gi < 0 || gi >= 10 * 10 * 10)
		if (gi < 0)
			return;

		//if (grid[gi].empty())
		size_t p_index = grid[gi];

		if (p_index == 0)
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

				// ignore if outside the grid
				//if (gi < 0 || gi >= 10 * 10 * 10)
				if (gi < 0)
					continue;

				// ignore if already inside the queue
				if (visited_statuses[gi])
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

			//for (int p_index : grid[gi])
			p_index -= 1;

			vec4 node = cells->at(p_index).node.lift();

			bool clipped = false;

			// ignore if cell is invisible (clipped by the clipping plane)
			for (const vec4& cp : *clipping_planes)
			{
				if (dot(node, cp) < 0)
				{
					clipped = true;
					break;
				}
			}

			if (!clipped)
				res.consider(p_index, (cells->at(p_index).node - q).sqr_length());
		}
	}

	result_entry closest_point(const vec3& q) const
	{
		{
			std::lock_guard<std::mutex> lock(mutex);

			if (!build_grid && grid != NULL && visited_statuses != NULL)
			{
				memset(visited_statuses, false, sizeof(bool) * extents.x() * extents.y() * extents.z());

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
		}

		return result_entry();
	}

	void set_clipping_planes(const std::vector<vec4>* _clipping_planes)
	{
		clipping_planes = _clipping_planes;
	}

	void print() const
	{
		for (int i = 0; i < extents.x(); ++i)
		{
			for (int j = 0; j < extents.y(); ++j)
			{
				for (int k = 0; k < extents.z(); ++k)
				{
					int gi = get_cell_index_to_grid_index(ivec3(i, j, k));

					//if (grid[gi].empty())
					size_t p_index = grid[gi];
					if (p_index == 0)
						continue;

					p_index -= 1;

					std::cout << "Grid[" << i << ", " << j << ", " << k << "]" << std::endl;

					std::cout << cells->at(p_index).node << std::endl;

					//for (size_t index : grid[gi])
					//{
					//	std::cout << cells->at(index).node << std::endl;
					//}

					//if (grid[gi].size() > 1)
					//{
					//	std::cout << "Nodes overlap at [" << i << ", " << j << ", " << k << "]" << std::endl;
					//}

					std::cout << "=============" << std::endl;
				}
			}
		}
	}

	void cancel_build_from_vertices()
	{
		{
			std::lock_guard<std::mutex> lock(mutex);

			build_grid = false;
		}

		if (thread.joinable())
			thread.join();
	}

	void build_from_vertices(const std::vector<T>* _cells, size_t _cells_start, size_t _cells_end, const ivec3& _extents, bool print_grid = false)
	{
		{
			std::lock_guard<std::mutex> lock(mutex);

			reset(_extents);

			cells = _cells;

			current_cell_index = _cells_start;
			cells_end = _cells_end;

			if (cells == NULL)
				return;

			//auto start = std::chrono::high_resolution_clock::now();

			//size_t index = 0;
			//for (std::vector<T>::const_iterator vit = cells->begin(), vend = cells->end(); vit != vend; ++vit)
			//	insert(index++, vit->node);

			//auto stop = std::chrono::high_resolution_clock::now();

			//auto duration = std::chrono::duration_cast<std::chrono::microseconds > (stop - start);

			//std::cout << "Time taken by single-threaded build_from_vertices: " << duration.count() << " microseconds" << std::endl;

			build_grid = true;
		}

		thread = std::thread(&regular_grid::build_from_vertices_impl, this, print_grid);
	}
};
