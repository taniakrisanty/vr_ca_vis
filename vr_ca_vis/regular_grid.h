// This source code is property of the Computer Graphics and Visualization 
// chair of the TU Dresden. Do not distribute! 
// Copyright (C) CGV TU Dresden - All Rights Reserved

#pragma once

#include <unordered_map>
#include <array>
#include <map>
#include <vector>

#include <cgv/render/render_types.h>

typedef cgv::math::fvec<float, 3> vec3;

class regular_grid
{
private:
	static const int dimension = 100;

	int grid[dimension * dimension * dimension];

public:
	regular_grid()
	{
	}

	void clear()
	{
		memset(grid, 0, dimension * dimension * dimension);
	}

	//converts a position to a grid index
	int PositionToIndex(const vec3& pos) const
	{
		return pos.x() + pos.y() * dimension + pos.z() * dimension * dimension;
	}

	int GetFromPosition(const vec3& pos) const
	{
		int index = PositionToIndex(pos);

		if (index < dimension * dimension * dimension)
		{
			return grid[index];
		}
		else
		{
			return 0;
		}
	}
	
	////return the center position of a cell specified by its cell key
	//Eigen::Vector3f CellCenter(const ivec3& idx) const
	//{
	//	Eigen::Vector3f p;
	//	for (int d = 0; d < 3; ++d)
	//		p[d] = (idx[d] + 0.5f) * cellExtents[d];

	//	return p;
	//}

	////return the center position of a cell containing give position pos
	//Eigen::Vector3f CellCenter(const Eigen::Vector3f& pos) const
	//{
	//	return CellCenter(PositionToIndex(pos));
	//}

	////return the min corner position of a cell specified by its cell key
	//Eigen::Vector3f CellMinPosition(const ivec3& key) const
	//{
	//	Eigen::Vector3f p;
	//	for (int d = 0; d < 3; ++d)
	//		p[d] = key[d] * cellExtents[d];

	//	return p;
	//}

	////return the min corner position of a cell containing the point pos
	//Eigen::Vector3f CellMinPosition(const Eigen::Vector3f& pos) const
	//{
	//	return CellMinPosition(PositionToIndex(pos));
	//}

	////return the max corner position of a cell specified by its cell key
	//Eigen::Vector3f CellMaxPosition(const ivec3& idx) const
	//{
	//	Eigen::Vector3f p;
	//	for (int d = 0; d < 3; ++d)
	//		p[d] = (idx[d] + 1) * cellExtents[d];

	//	return p;
	//}

	////return the max corner position of a cell containing the point pos
	//Eigen::Vector3f CellMaxPosition(const Eigen::Vector3f& pos) const
	//{
	//	return CellMaxPosition(PositionToIndex(pos));
	//}

	////returns bounding box of cell with index idx
	//Box CellBounds(const ivec3& idx) const
	//{
	//	return Box(CellMinPosition(idx), CellMaxPosition(idx));
	//}

	////returns the  bounding box of cell containing the point pos
	//Box CellBounds(const Eigen::Vector3f& pos) const
	//{
	//	ivec3 idx = PositionToIndex(pos);
	//	return Box(CellMinPosition(idx), CellMaxPosition(idx));
	//}

	////returns the extents of a grid cell
	//Eigen::Vector3f CellExtents() const
	//{
	//	return cellExtents;
	//}

	////returns volume of a grid cell
	//float CellVolume() const
	//{
	//	float vol = 0;
	//	for (int d = 0; d < 3; ++d)
	//		vol *= cellExtents[d];
	//	return vol;
	//}

	////removes all non empty cells from the hash grid
	//bool Empty(const ivec3& idx) const
	//{
	//	auto it = cellHashMap.find(idx);
	//	if (it == cellHashMap.end())
	//		return true;
	//	return false;
	//}

	//inserts primitive p into all overlapping hash grid cells
	void Insert(int p_index, const vec3& p)
	{
		int index = PositionToIndex(p);
		
		grid[index] = p_index + 1;
	}

	////remove all cells from hash grid
	//void Clear()
	//{
	//	cellHashMap.clear();
	//}

	////returns true if hashgrid contains no cells
	//bool Empty() const
	//{
	//	return cellHashMap.empty();
	//}

	////returns the number of non empty cells
	//size_t NumCells() const
	//{
	//	return cellHashMap.size();
	//}

	////iterator pointing to the  first cell within the hashgrid
	//typename CellHashMapType::iterator NonEmptyCellsBegin()
	//{
	//	return cellHashMap.begin();
	//}

	////iterator pointing behind the last cell within the hashgrid
	//typename CellHashMapType::iterator NonEmptyCellsEnd()
	//{
	//	return cellHashMap.end();
	//}

	////const iterator pointing to the first cell within the hashgrid
	//typename CellHashMapType::const_iterator NonEmptyCellsBegin() const
	//{
	//	return cellHashMap.begin();
	//}

	////const iterator pointing behind the last cell within the hashgrid
	//typename CellHashMapType::const_iterator NonEmptyCellsEnd() const
	//{
	//	return cellHashMap.end();
	//}

	////iterator pointing to the first primitive stored in the cell idx
	//typename std::vector<Primitive>::iterator PrimitivesBegin(const ivec3& idx)
	//{
	//	assert(!Empty(idx));
	//	return cellHashMap[idx].begin();
	//}

	////iterator pointing after the last primitive stored in the cell idx
	//typename std::vector<Primitive>::iterator PrimitivesEnd(const ivec3& idx)
	//{
	//	assert(!Empty(idx));
	//	return cellHashMap[idx].end();
	//}

	////const iterator pointing to the first primitive stored in the cell idx
	//typename std::vector<Primitive>::const_iterator PrimitivesBegin(const ivec3& idx) const
	//{
	//	assert(!Empty(idx));
	//	return cellHashMap[idx].cbegin();
	//}

	////const iterator pointing after the last primitive stored in the cell idx
	//typename std::vector<Primitive>::const_iterator PrimitivesEnd(const ivec3& idx) const
	//{
	//	assert(!Empty(idx));
	//	return cellHashMap[idx].cend();
	//}
};

//helper function to construct a hashgrid data structure from the vertices of the halfedge mesh m
void BuildRegularGridFromVertices(const std::vector<vec3>& points, regular_grid& grid);
