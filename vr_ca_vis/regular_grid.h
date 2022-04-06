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
private:
	mutable std::mutex mutex;
	std::thread thread;

	bool build_grid = false;

	size_t* cell_grid = NULL;
	size_t* node_grid = NULL;

	bool* visited_statuses = NULL;

	vec3 extents;
	vec3 cell_extents;

	const std::vector<T>* cells = NULL;
	size_t current_cell_index, cells_end;

	void reset(const ivec3& _extents)
	{
		vec3 e(_extents[0] / cell_extents[0], _extents[1] / cell_extents[1], _extents[2] / cell_extents[2]);

		size_t count = int(e.x() * e.y() * e.z());

		if (e != extents)
		{
			if (cell_grid) delete[] cell_grid;
			if (node_grid) delete[] node_grid;
			if (visited_statuses) delete[] visited_statuses;

			cell_grid = node_grid = NULL;
			visited_statuses = NULL;

			extents = e;

			if (count > 0) {
				cell_grid = new size_t[count];
				node_grid = new size_t[count];
				visited_statuses = new bool[count];
			}
		}

		if (count > 0) {
			memset(cell_grid, 0, sizeof(size_t) * count);
			memset(node_grid, 0, sizeof(size_t) * count);
			memset(visited_statuses, false, sizeof(bool) * count);
		}
	}

	//return the center position of a cell specified by its cell key
	vec3 get_cell_center(const ivec3& idx) const
	{
		vec3 p;
		for (int d = 0; d < 3; ++d)
			p[d] = (idx[d] + 0.5f) * cell_extents[d];

		return p;
	}

	//converts a position to a grid index
	int get_position_to_grid_index(const vec3& pos) const
	{
		ivec3 ci = get_position_to_cell_index(pos, cell_extents);

		return get_cell_index_to_grid_index(ci);
	}

	//inserts node with node_index in cell with index cell_index into all overlapping regular grid cells
	void insert(int cell_index, int node_index, const vec3& p)
	{
		int gi = get_position_to_grid_index(p);

		cell_grid[gi] = cell_index + 1;
		node_grid[gi] = node_index + 1;
	}

	void build_from_vertices_impl(bool print_grid = false)
	{
#ifdef DEBUG
		auto start = std::chrono::high_resolution_clock::now();
#endif

		while (true)
		{
			{
				std::lock_guard<std::mutex> lock(mutex);

				if (!build_grid)
				{
#ifdef DEBUG
					std::cout << "regular_grid::build_from_vertices cancelled at index " << current_cell_index << std::endl;
#endif
					break;
				}

				if (current_cell_index == cells_end)
				{
#ifdef DEBUG
					auto stop = std::chrono::high_resolution_clock::now();

					auto duration = std::chrono::duration_cast<std::chrono::microseconds>(stop - start);

					std::cout << "regular_grid::build_from_vertices finished in " << duration.count() << " microseconds" << std::endl;
#endif

					build_grid = false;

					if (print_grid) print();
					break;
				}

				const auto& c = (*cells)[current_cell_index];

				for (size_t i = c.nodes_start_index; i < c.nodes_end_index; ++i)
					insert(current_cell_index, i - c.nodes_start_index, cell::nodes[i]);

				++current_cell_index;
			}

			std::this_thread::yield();
		}
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

					size_t cell_index = cell_grid[gi];
					if (cell_index == 0)
						continue;

					size_t node_index = node_grid[gi];
					if (node_index == 0)
						continue;

					cell_index -= 1;
					node_index -= 1;

					std::cout << "Grid[" << i << ", " << j << ", " << k << "]" << std::endl;

					const cell& c = (*cells)[cell_index];

					std::cout << cell::nodes[c.nodes_start_index + node_index] << std::endl;

					std::cout << "=============" << std::endl;
				}
			}
		}
	}

public:
	regular_grid(const float _cell_extents = 1.f) : current_cell_index(0), cells_end(0)
	{
		cell_extents[0] = cell_extents[1] = cell_extents[2] = _cell_extents;
	}

	int get_cell_index_to_grid_index(const ivec3& ci) const
	{
		if (ci.x() < 0 || ci.x() >= extents.x() ||
			ci.y() < 0 || ci.y() >= extents.y() ||
			ci.z() < 0 || ci.z() >= extents.z())
			return -1;

		// TODO check
		return ci.x() + ci.y() * int(extents.x()) + ci.z() * int(extents.y() * extents.z());
	}

	//return the center position of a cell containing give position pos
	//vec3 get_cell_center(const vec3& pos) const
	//{
	//	return get_cell_center(get_position_to_cell_index(pos, cell_extents));
	//}

	bool get_closest_index(int gi, size_t& cell_index, size_t& node_index) const
	{
		if (gi < 0 || gi >= extents.x() * extents.y() * extents.z()) // grid cell index is out of bounds
			return false;

		std::lock_guard<std::mutex> lock(mutex);

		if (build_grid)
			return false;

		if (cell_grid == NULL || node_grid == NULL)
			return false;

		size_t c_index = cell_grid[gi];
		if (c_index == 0)
			return false;

		size_t n_index = node_grid[gi];
		if (n_index == 0)
			return false;

		cell_index = c_index - 1;
		node_index = n_index - 1;

		return true;
	}

	//returns the extents of a grid cell
	vec3 get_cell_extents() const
	{
		return cell_extents;
	}

	void remove_outermost(const vec3& pos, size_t cell_index, size_t node_index, std::vector<size_t>& cell_indices, std::vector<size_t>& node_indices) const
	{
		std::lock_guard<std::mutex> lock(mutex);

		std::vector<int> removed_gi;

		if (!build_grid && cell_grid != NULL && node_grid != NULL && visited_statuses != NULL)
		{
			memset(visited_statuses, false, sizeof(bool) * int(extents.x() * extents.y() * extents.z()));

			ivec3 ci = get_position_to_cell_index(pos, cell_extents);

			int gi = get_cell_index_to_grid_index(ci);

			// ignore if outside the grid
			if (gi < 0)
				return;

			size_t c_index = cell_grid[gi], n_index = node_grid[gi];

			if (c_index == 0 || n_index == 0)
				return;

			if (c_index != cell_index + 1 || n_index != node_index + 1)
				return;

			std::queue<ivec3> gc_indices;

			visited_statuses[gi] = true;

			gc_indices.push(ci);

			do
			{
				ci = gc_indices.front();
				gc_indices.pop();

				std::cout << ci << std::endl;

				int gi = get_cell_index_to_grid_index(ci);

				size_t neighbors = 0;

				std::vector<ivec3> cis{
					ivec3(ci.x() - 1, ci.y(), ci.z()),	// left
					ivec3(ci.x() + 1, ci.y(), ci.z()),	// right
					ivec3(ci.x(), ci.y() - 1, ci.z()),	// bottom
					ivec3(ci.x(), ci.y() + 1, ci.z()),	// top
					ivec3(ci.x(), ci.y(), ci.z() - 1),	// back
					ivec3(ci.x(), ci.y(), ci.z() + 1)	// front
				};

				for (const ivec3& _ci : cis)
				{
					int _gi = get_cell_index_to_grid_index(_ci);

					// ignore if outside the grid
					if (_gi < 0)
						continue;

					size_t _c_index = cell_grid[_gi], _n_index = node_grid[_gi];

					if (_c_index == 0 || _n_index == 0)
						continue;

					neighbors += 1;
				}

				if (neighbors == 6)
					continue;

				removed_gi.push_back(gi);

				cell_indices.push_back(cell_grid[gi] - 1);
				node_indices.push_back(node_grid[gi] - 1);

				cis.push_back(ivec3(ci.x() - 1, ci.y() - 1, ci.z()));	// left - bottom
				cis.push_back(ivec3(ci.x() - 1, ci.y() + 1, ci.z()));	// left - top
				cis.push_back(ivec3(ci.x() + 1, ci.y() - 1, ci.z()));	// right - bottom
				cis.push_back(ivec3(ci.x() + 1, ci.y() + 1, ci.z()));	// right - top
				cis.push_back(ivec3(ci.x(), ci.y() - 1, ci.z() - 1));	// bottom - back
				cis.push_back(ivec3(ci.x(), ci.y() - 1, ci.z() + 1));	// bottom - front
				cis.push_back(ivec3(ci.x(), ci.y() + 1, ci.z() - 1));	// top - back
				cis.push_back(ivec3(ci.x(), ci.y() + 1, ci.z() + 1));	// top - front

				for (const ivec3& _ci : cis)
				{
					int _gi = get_cell_index_to_grid_index(_ci);

					// ignore if outside the grid
					if (_gi < 0)
						continue;

					size_t _c_index = cell_grid[_gi], _n_index = node_grid[_gi];

					if (_c_index == 0 || _n_index == 0)
						continue;

					// ignore if already inside the queue
					if (visited_statuses[_gi])
						continue;

					visited_statuses[_gi] = true;
					
					gc_indices.push(_ci);
				}
			} while (!gc_indices.empty());

			for (size_t i : removed_gi) {
				cell_grid[i] = 0;
				node_grid[i] = 0;
			}
		}
	}

	void build_from_vertices(const std::vector<T>* _cells, size_t _cells_start, size_t _cells_end, const ivec3& _extents = ivec3(0), bool print_grid = false)
	{
		{
			std::lock_guard<std::mutex> lock(mutex);

			reset(_extents);

			cells = _cells;

			current_cell_index = _cells_start;
			cells_end = _cells_end;

			if (cells == NULL)
				return;

			build_grid = true;
		}

		thread = std::thread(&regular_grid::build_from_vertices_impl, this, print_grid);
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
};
