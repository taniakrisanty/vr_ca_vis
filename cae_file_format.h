#pragma once

#include <string>
#include <cstdint>
#include <cstdio>
#include <cgv/utils/file.h>
#include <cgv/math/fvec.h>
#include <cgv/media/axis_aligned_box.h>
#include "endian.h"

namespace cae {

enum CoordinateType
{
	CT_UINT8,
	CT_UINT16,
	CT_UINT32,
	CT_UINT64,
	CT_INT8,
	CT_INT16,
	CT_INT32,
	CT_INT64,
	CT_FLT32,
	CT_FLT64
};

const size_t type_sizes[]{
	sizeof(uint8_t),
	sizeof(uint16_t),
	sizeof(uint32_t),
	sizeof(uint64_t),
	sizeof(int8_t),
	sizeof(int16_t),
	sizeof(int32_t),
	sizeof(int64_t),
	sizeof(float),
	sizeof(double)
};

const float type_min_values[]{
	float(std::numeric_limits<uint8_t>::min()),
	float(std::numeric_limits <uint16_t>::min()),
	float(std::numeric_limits<uint32_t>::min()),
	float(std::numeric_limits<uint64_t>::min()),
	float(std::numeric_limits<int8_t>::min()),
	float(std::numeric_limits<int16_t>::min()),
	float(std::numeric_limits<int32_t>::min()),
	float(std::numeric_limits<int64_t>::min()),
	0.0f,
	0.0f
};

const float type_max_values[]{
	float(std::numeric_limits<uint8_t>::max()),
	float(std::numeric_limits<uint16_t>::max()),
	float(std::numeric_limits<uint32_t>::max()),
	float(std::numeric_limits<uint64_t>::max()),
	float(std::numeric_limits<int8_t>::max()),
	float(std::numeric_limits<int16_t>::max()),
	float(std::numeric_limits<int32_t>::max()),
	float(std::numeric_limits<int64_t>::max()),
	1.0f,
	1.0f
};

template <typename T>
void type_init(void* void_ptr, size_t cnt) {
	T* ptr = reinterpret_cast<T*>(void_ptr);
	for (size_t i = 0; i < cnt; ++i)
		ptr[i] = T(0);
}

typedef void(*type_init_type)(void*, size_t);

const type_init_type type_inits[] = {
	&type_init<uint8_t>,
	&type_init<uint16_t>,
	&type_init<uint32_t>,
	&type_init<uint64_t>,
	&type_init<int8_t>,
	&type_init<int16_t>,
	&type_init<int32_t>,
	&type_init<int64_t>,
	&type_init<float>,
	&type_init<double>
};

template <typename T>
struct coordinate_traits
{
	static const CoordinateType type = CoordinateType(-1);
};

template <> struct coordinate_traits<uint8_t>  { static const CoordinateType type = CT_UINT8; };
template <> struct coordinate_traits<uint16_t> { static const CoordinateType type = CT_UINT16; };
template <> struct coordinate_traits<uint32_t> { static const CoordinateType type = CT_UINT32; };
template <> struct coordinate_traits<uint64_t> { static const CoordinateType type = CT_UINT64; };
template <> struct coordinate_traits<int8_t>   { static const CoordinateType type = CT_INT8; };
template <> struct coordinate_traits<int16_t>  { static const CoordinateType type = CT_INT16; };
template <> struct coordinate_traits<int32_t>  { static const CoordinateType type = CT_INT32; };
template <> struct coordinate_traits<int64_t>  { static const CoordinateType type = CT_INT64; };
template <> struct coordinate_traits<float>    { static const CoordinateType type = CT_FLT32; };
template <> struct coordinate_traits<double>   { static const CoordinateType type = CT_FLT64; };

template <typename S, typename D>
void convert_coordinate_vector(const S* src, D* dst, size_t cnt)
{
	for (size_t i = 0; i < cnt; ++i)
		dst[i] = D(src[i]);
}

template <typename S>
void convert_from_coordinate_vector(const S* src, const S& src_min, const S& src_max,
	float* dst, const std::vector<cgv::math::fvec<float,2> >& ranges, size_t cnt)
{
	for (size_t i = 0; i < cnt; ++i) {
		const cgv::math::fvec<float, 2>& r = ranges[i%ranges.size()];
		dst[i] = float((r[1] - r[0]) / (src_max - src_min) * (src[i] - src_min) + r[0]);
	}
}

template <typename D>
void convert_to_coordinate_vector(D* dst, const D& dst_min, const D& dst_max,
	const float* src, const std::vector<cgv::math::fvec<float, 2> >& ranges, size_t cnt)
{
	for (size_t i = 0; i < cnt; ++i) {
		const cgv::math::fvec<float, 2>& r = ranges[i%ranges.size()];
		dst[i] = D((src[i] - r[0]) / (r[1] - r[0]) * (dst_max - dst_min) + dst_min);
	}
}


struct FileVersion
{
	char file_type[3]; // "cae" cell automaton evolution
	uint8_t major_version : 4; // first version is 1.0
	uint8_t minor_version : 4;
};

enum FileFlags
{
	FF_NONE = 0,
	FF_COORDINATE_INTERVALS = 1, // store for x, y, and z coordinates the min values followed by the max values arising in the dataset
	FF_ATTRIBUTE_RANGES = 2,     // store for each attribute the ranges that they assume and to which they should be transformed
	FF_FRAME_BASED = 4,          // data is ordered frame by frame
	FF_SEPARATE_FRAME_FILE = 8   // frame data is stored in a separate file with extension "caf"
};

struct FileFormat
{
	uint8_t endian : 2;
	uint8_t flags  : 6;
	uint8_t point_coord_type;
	uint8_t group_index_type;
	uint8_t attribute_type;
};

// fixed length part of header
struct binary_header_lead
{
	FileVersion version;
	FileFormat format;
	uint64_t nr_points;
	uint32_t nr_groups;
	uint32_t nr_time_steps;
	uint32_t nr_attributes;
	mutable uint32_t total_nr_chars_in_attribute_names;
};

class binary_header : public binary_header_lead
{
public:
	typedef cgv::math::fvec<float, 2> vec2;
	typedef cgv::math::fvec<float, 3> vec3;
protected:
	template <typename T>
	bool read_vector(FILE* fp, std::vector<T>& V, size_t cnt) const
	{
		V.resize(cnt);
		if (fread(&V.front(), sizeof(T), cnt, fp) != cnt)
			return false;
		Endian file = Endian(format.endian);
		Endian machine = get_endian();
		map_convert_endian(V, file, machine);
		return true;
	}
	template <typename T>
	bool write_vector(FILE* fp, const std::vector<T>& V) const
	{
		Endian file = Endian(format.endian);
		Endian machine = get_endian();
		map_convert_endian(const_cast<std::vector<T>&>(V), machine, file);
		bool success = fwrite(&V.front(), sizeof(T), V.size(), fp) == V.size();
		map_convert_endian(const_cast<std::vector<T>&>(V), file, machine);
		return success;
	}
	///
	void convert_vector_void(CoordinateType src_type, const void* src_ptr, CoordinateType dst_type, void* dst_ptr, size_t cnt, bool is_attr = false) const;
	/// pointer to six values of the point coordinate type containing (min_x,min_y,min_z,max_x,max_y,max_z) 
	mutable void* coordinate_intervals;
	/// ensure that the coordinate intervals are allocated with the size needed by the point coordinate type stored in format member
	size_t ensure_coordinate_intervals_allocated() const;
	/// ensure that attribute ranges are allocated and initialized to invalid
	void ensure_attribute_ranges();
public:
	size_t get_header_size() const;
	size_t get_entry_size() const;
	/// names of attributes
	std::vector<std::string> attr_names;
	/// set the coordinate intervals from values of an arbitrary type
	template <typename T>
	void set_coordinate_intervals(T x_min, T y_min, T z_min, T x_max, T y_max, T z_max) {
		T values[6] = { x_min, y_min, z_min, x_max, y_max, z_max };
		ensure_coordinate_intervals_allocated();
		convert_vector_void(coordinate_traits<T>::type, values, CoordinateType(format.point_coord_type), coordinate_intervals, 6);
	}
	/// return point with min coordinates
	vec3 get_min_point() const;
	/// return point with min coordinates
	vec3 get_max_point() const;
	/// float (min,max) range for each attribute used to map integer types to floats
	std::vector<vec2> attribute_ranges;
	/// time in seconds for each time step
	std::vector<float> times;
	/// global index of first particle for each time step
	std::vector<uint64_t> time_step_start;
	/// return the end of a time step
	uint64_t get_time_step_end(size_t ti) const { return (ti + 1 == time_step_start.size()) ? nr_points : time_step_start[ti + 1]; }
	/// constructor initializes all fields
	binary_header(FileFlags _flags = FF_NONE);
	/// deallocate coordinate_intervals
	~binary_header();
	/// read header information. If parameter fpp is given keep file open and pass file pointer back through fpp parameter.
	bool read_header(const std::string& file_name, FILE** fpp = 0);
	/// write header information. If parameter fpp is given keep file open and pass file pointer back through fpp parameter.
	bool write_header(const std::string& file_name, FILE** fpp = 0) const;
};

class binary_file : public binary_header
{
protected:
	bool read_variant_vector_void(FILE* fp, void* values, size_t cnt, CoordinateType file_type, CoordinateType value_type, bool is_attr = false) const;

	template <typename T>
	bool read_variant_vector(FILE* fp, std::vector<T>& V, size_t cnt, CoordinateType file_type, bool is_attr = false) const
	{
		V.resize(cnt);
		return read_variant_vector_void(fp, &V.front(), cnt, file_type, coordinate_traits<T>::type, is_attr);
	}
	template <typename T>
	bool read_variant_vector(FILE* fp, std::vector<cgv::math::fvec<T, 3> >& V, size_t cnt, CoordinateType file_type) const
	{
		V.resize(cnt);
		return read_variant_vector_void(fp, &V.front(), 3 * cnt, file_type, coordinate_traits<T>::type);
	}

	bool write_variant_vector_void(FILE* fp, const void* values, size_t cnt, CoordinateType file_type, CoordinateType value_type, bool is_attr = false) const;

	template <typename T>
	bool write_variant_vector(FILE* fp, const std::vector<T>& V, CoordinateType file_type, bool is_attr = false) const
	{
		return write_variant_vector_void(fp, &V.front(), V.size(), file_type, coordinate_traits<T>::type, is_attr);
	}
	template <typename T>
	bool write_variant_vector(FILE* fp, const std::vector<cgv::math::fvec<T, 3> >& V, CoordinateType file_type) const
	{
		return write_variant_vector_void(fp, &V.front(), 3 * V.size(), file_type, coordinate_traits<T>::type);
	}

	bool read_time_step_void(const std::string& file_name, uint64_t beg, uint64_t cnt,
		void* pnt_ptr, CoordinateType pnt_type,
		void* grp_ptr, CoordinateType grp_type,
		void* att_ptr, CoordinateType att_type) const;

	bool write_time_step_void(const std::string& file_name, uint64_t cnt,
		const void* pnt_ptr, CoordinateType pnt_type,
		const void* grp_ptr, CoordinateType grp_type,
		const void* att_ptr, CoordinateType att_type, bool write_hdr) const;

public:
	template <typename P, typename I, typename A>
	bool read(const std::string& file_name, 
		      std::vector<cgv::math::fvec<P,3> >& points,
			  std::vector<I>& group_indices, 
		      std::vector<A>& attr_values)
	{
		FILE* fp = 0;
		if (!read_header(file_name+".cae", &fp))
			return false;
		
		if ((format.flags & FF_SEPARATE_FRAME_FILE) != 0) {
			fclose(fp);
			fp = fopen((file_name + ".caf").c_str(), "rb");
			if (!fp)
				return false;
		}
		if ((format.flags & FF_FRAME_BASED) == 0) {
			bool success =
				read_variant_vector(fp, points, size_t(nr_points), CoordinateType(format.point_coord_type)) &&
				read_variant_vector(fp, group_indices, size_t(nr_points), CoordinateType(format.group_index_type)) &&
				read_variant_vector(fp, attr_values, size_t(nr_attributes*nr_points), CoordinateType(format.attribute_type));
			fclose(fp);
			return success;
		}
		else {
			fclose(fp);
			std::vector<cgv::math::fvec<P, 3> > tmp_points;
			std::vector<I> tmp_group_indices;
			std::vector<A> tmp_attr_values;
			for (size_t ti = 0; ti < nr_time_steps; ++ti) {
				if (!read_time_step(file_name, uint32_t(ti), tmp_points, tmp_group_indices, tmp_attr_values))
					return false;
				points.insert(points.end(), tmp_points.begin(), tmp_points.end());
				group_indices.insert(group_indices.end(), tmp_group_indices.begin(), tmp_group_indices.end());
				attr_values.insert(attr_values.end(), tmp_attr_values.begin(), tmp_attr_values.end());
			}
			return true;
		}
	}

	template <typename P, typename I, typename A>
	bool write(const std::string& file_name,
		       const std::vector<cgv::math::fvec<P, 3> >& points,
		       const std::vector<I>& group_indices,
		       const std::vector<A>& attr_values) const
	{
		FILE* fp = 0;
		if (!write_header(file_name, &fp))
			return false;
		
		if ((format.flags & FF_SEPARATE_FRAME_FILE) != 0) {
			fclose(fp);
			fp = fopen((file_name + ".caf").c_str(), "wb");
			if (!fp)
				return false;
		}
		if ((format.flags & FF_FRAME_BASED) == 0) {
			bool success =
				write_variant_vector(fp, points, CoordinateType(format.point_coord_type)) &&
				write_variant_vector(fp, group_indices, CoordinateType(format.group_index_type)) &&
				write_variant_vector(fp, attr_values, CoordinateType(format.attribute_type));

			fclose(fp);
			return success;
		}
		else {
			fclose(fp);
			for (size_t ti = 0; ti < nr_time_steps; ++ti) {
				size_t beg = size_t(time_step_start[ti]);
				size_t end = size_t(get_time_step_end(ti));
				std::vector<cgv::math::fvec<P, 3> > tmp_points;
				std::vector<I> tmp_group_indices;
				std::vector<A> tmp_attr_values;
				tmp_points.insert(tmp_points.end(), points.begin() + beg, points.begin() + end);
				tmp_group_indices.insert(tmp_group_indices.end(), group_indices.begin() + beg, group_indices.begin() + end);
				tmp_attr_values.insert(tmp_attr_values.end(), attr_values.begin() + beg, attr_values.begin() + end);
				if (!write_time_step(file_name, tmp_points, tmp_group_indices, tmp_attr_values))
					return false;
			}
			return true;
		}
	}
	/// read a single time step
	template <typename P, typename I, typename A>
	bool read_time_step(const std::string& file_name, uint32_t ti,
		std::vector<cgv::math::fvec<P, 3> >& points,
		std::vector<I>& group_indices,
		std::vector<A>& attr_values)
	{
		uint64_t beg = time_step_start[ti];
		uint64_t end = ((ti + 1 == time_step_start.size()) ? size_t(nr_points) : time_step_start[ti + 1]);
		uint64_t cnt = end - beg;

		points.resize(size_t(cnt));
		group_indices.resize(size_t(cnt));
		attr_values.resize(size_t(cnt*nr_attributes));

		return read_time_step_void(file_name, beg, cnt,
			&points.front(), coordinate_traits<P>::type,
			&group_indices.front(), coordinate_traits<I>::type,
			&attr_values.front(), coordinate_traits<A>::type);
	}
	/// read a single time step
	template <typename P, typename I, typename A>
	bool append_time_step(const std::string& file_name, float time,
		const std::vector<cgv::math::fvec<P, 3> >& points,
		const std::vector<I>& group_indices,
		const std::vector<A>& attr_values, bool update_statistics = true, bool write_hdr = false)
	{
		assert(points.size() == group_indices.size());
		assert(nr_attributes*points.size() == attr_values.size());
		assert((format.flags & FF_FRAME_BASED) != 0);
		time_step_start.push_back(nr_points);
		times.push_back(time);
		nr_points += points.size();
		if (update_statistics) {
			// update coordinate_intervals only if corresponding file flag is set
			if ((format.flags & FF_COORDINATE_INTERVALS) != 0) {
				ensure_coordinate_intervals_allocated();
				cgv::media::axis_aligned_box<P, 3> box;
				convert_vector_void(CoordinateType(format.point_coord_type), coordinate_intervals, coordinate_traits<P>::type, &box, 6);
				for (const auto& p : points)
					box.add_point(p);
				convert_vector_void(coordinate_traits<P>::type, &box, CoordinateType(format.point_coord_type), coordinate_intervals, 6);
			}
			// always update nr_groups
			for (auto idx : group_indices)
				if (idx + 1 > nr_groups)
					nr_groups = idx + 1;
			// update attribute ranges only if corresponding file flag is set
			if ((format.flags & FF_ATTRIBUTE_RANGES) != 0) {
				ensure_attribute_ranges();
				std::vector<float> float_attr_values;
				const float* float_attr_value_ptr;
				if (coordinate_traits<A>::type == CT_FLT32)
					float_attr_value_ptr = &attr_values[0];
				else {
					float_attr_values.resize(points.size()*nr_attributes);
					float_attr_value_ptr = &float_attr_values[0];
					convert_vector_void(coordinate_traits<A>::type, &attr_values.front(), CT_FLT32, &float_attr_values[0], attr_values.size());
				}
				for (size_t i = 0; i < attr_values.size(); ++i) {
					float f = float_attr_value_ptr[i];
					vec2& r = attribute_ranges[i%nr_attributes];
					if (r[1] < r[0])
						r[0] = r[1] = f;
					else {
						if (f < r[0])
							r[0] = f;
						if (f > r[1])
							r[1] = f;
					}
				}
			}
		}
		return write_time_step(file_name, points, group_indices, attr_values, write_hdr);
	}
	/// read a single time step
	template <typename P, typename I, typename A>
	bool write_time_step(const std::string& file_name, 
		const std::vector<cgv::math::fvec<P, 3> >& points,
		const std::vector<I>& group_indices,
		const std::vector<A>& attr_values, bool write_hdr = false) const
	{
		return write_time_step_void(file_name, points.size(),
			&points.front(), coordinate_traits<P>::type,
			&group_indices.front(), coordinate_traits<I>::type,
			&attr_values.front(), coordinate_traits<A>::type, write_hdr);
	}
};

}
