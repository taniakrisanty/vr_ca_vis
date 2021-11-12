#include "cae_file_format.h"

namespace cae {

	size_t binary_header::ensure_coordinate_intervals_allocated() const
	{
		size_t coord_size = type_sizes[format.point_coord_type];
		coordinate_intervals = new uint8_t[6 * coord_size];
		type_inits[format.point_coord_type](coordinate_intervals, 6);
		return coord_size;
	}
	/// ensure that attribute ranges are allocated and initialized to invalid
	void binary_header::ensure_attribute_ranges()
	{
		if (attribute_ranges.size() == nr_attributes)
			return;
		attribute_ranges.clear();
		attribute_ranges.resize(nr_attributes, vec2(1.0f, 0.0f));
	}

	size_t binary_header::get_header_size() const
	{
		size_t size = sizeof(binary_header_lead);
		size += total_nr_chars_in_attribute_names;
		size += sizeof(uint16_t)*nr_attributes;
		size += sizeof(times.front())*nr_time_steps;
		size += sizeof(time_step_start.front())*nr_time_steps;
		if ((format.flags & FF_COORDINATE_INTERVALS) != 0)
			size += type_sizes[format.point_coord_type] * 6;
		if ((format.flags & FF_ATTRIBUTE_RANGES) != 0)
			size += sizeof(attribute_ranges.front())*nr_attributes;
		return size;
	}
	size_t binary_header::get_entry_size() const
	{
		return 3 * type_sizes[format.point_coord_type] +
			type_sizes[format.group_index_type] +
			type_sizes[format.attribute_type] * nr_attributes;
	}
	/// constructor initializes all fields
	binary_header::binary_header(FileFlags _flags)
	{
		version.file_type[0] = 'c';
		version.file_type[1] = 'a';
		version.file_type[2] = 'e';
		version.major_version = 1;
		version.minor_version = 0;
		format.endian = get_endian();
		format.flags = _flags;
		format.point_coord_type = CT_UINT8;
		format.group_index_type = CT_UINT8;
		format.attribute_type = CT_FLT32;
		nr_points = 0;
		nr_groups = 0;
		nr_time_steps = 0;
		nr_attributes = 0;
		total_nr_chars_in_attribute_names = 0;
		coordinate_intervals = 0;
	}

	/// deallocate coordinate_intervals
	binary_header::~binary_header()
	{
		if (coordinate_intervals)
			delete[] reinterpret_cast<uint8_t*>(coordinate_intervals);
	}

	/// return point with min coordinates
	binary_header::vec3 binary_header::get_min_point() const
	{
		if (coordinate_intervals) {
			float crd_int_flt[3];
			convert_vector_void(CoordinateType(format.point_coord_type), coordinate_intervals, CT_FLT32, crd_int_flt, 3);
			return reinterpret_cast<vec3&>(crd_int_flt[0]);
		}
		float v = type_min_values[CoordinateType(format.point_coord_type)];
		return vec3(v, v, v);
	}
	/// return point with min coordinates
	binary_header::vec3 binary_header::get_max_point() const
	{
		if (coordinate_intervals) {
			float crd_int_flt[3];
			convert_vector_void(CoordinateType(format.point_coord_type), reinterpret_cast<uint8_t*>(coordinate_intervals)+3*type_sizes[format.point_coord_type], CT_FLT32, crd_int_flt, 3);
			return reinterpret_cast<vec3&>(crd_int_flt[0]);
		}
		float v = type_max_values[CoordinateType(format.point_coord_type)];
		return vec3(v,v,v);
	}
	/// read header information. If parameter fpp is given keep file open and pass file pointer back through fpp parameter.
	bool binary_header::read_header(const std::string& file_name, FILE** fpp)
	{
		// open file in binary mode
		FILE* fp = fopen(file_name.c_str(), "rb");
		if (!fp)
			return false;

		// read fixed length part of binary header
		if (fread(static_cast<binary_header_lead*>(this), sizeof(binary_header_lead), 1, fp) != 1) {
			fclose(fp);
			return false;
		}
		// check file type
		if (std::string(version.file_type, 3) != "cae") {
			std::cerr << "did expect file <" << file_name << "> to be of type cae" << std::endl;
			return false;
		}
		// check endianess and correct binary header lead if necessary
		Endian file_endian = Endian(format.endian);
		Endian machine_endian = get_endian();
		if (machine_endian != file_endian) {
			map_convert_endian(nr_points, file_endian, machine_endian);
			map_convert_endian(nr_groups, file_endian, machine_endian);
			map_convert_endian(nr_time_steps, file_endian, machine_endian);
			map_convert_endian(total_nr_chars_in_attribute_names, file_endian, machine_endian);
		}
		// allocate temporary data
		std::vector<uint16_t> name_lengths(nr_attributes);
		std::string all_names(total_nr_chars_in_attribute_names, ' ');

		// read variable length parts of header
		if ((fread(&all_names[0], 1, total_nr_chars_in_attribute_names, fp) != total_nr_chars_in_attribute_names) &&
			read_vector(fp, name_lengths, nr_attributes) &&
			read_vector(fp, times, nr_time_steps) &&
			read_vector(fp, time_step_start, nr_time_steps)) {

			// extract attrib names
			uint32_t char_offset = 0;
			for (auto length : name_lengths) {
				attr_names.push_back(all_names.substr(char_offset, length));
				char_offset += length;
			}

			// read optional stuff
			bool success = true;
			if ((format.flags & FF_COORDINATE_INTERVALS) != 0) {
				size_t coord_size = ensure_coordinate_intervals_allocated();
				if (fread(coordinate_intervals, coord_size, 6, fp) == 6)
					map_convert_endian(coordinate_intervals, 6, file_endian, machine_endian, uint32_t(coord_size));
				else
					success = false;
			}

			if (success && ((format.flags & FF_ATTRIBUTE_RANGES) != 0)) {
				if (!read_vector(fp, attribute_ranges, nr_attributes))
					success = false;
			}
			if (success) {
				// pass back file pointer or close file 
				if (fpp)
					*fpp = fp;
				else
					fclose(fp);

				return true;
			}
		}
		fclose(fp);
		return false;
	}
	/// write header information. If parameter fpp is given keep file open and pass file pointer back through fpp parameter.
	bool binary_header::write_header(const std::string& file_name, FILE** fpp) const
	{
		FILE* fp = fopen(file_name.c_str(), "wb");
		if (!fp)
			return false;

		// prepare fixed length part of binary header
		total_nr_chars_in_attribute_names = 0;

		// concatenate attribute names and collect attribute name lengths in vector
		std::string concatenated_attribute_names;
		std::vector<uint16_t> name_lengths;
		for (const auto& name : attr_names) {
			total_nr_chars_in_attribute_names += uint32_t(name.length());
			name_lengths.push_back(uint16_t(name.size()));
			concatenated_attribute_names += name;
		}

		Endian file_endian = Endian(format.endian);
		Endian machine_endian = get_endian();
		if (machine_endian != file_endian) {
			map_convert_endian(const_cast<binary_header*>(this)->nr_points, machine_endian, file_endian);
			map_convert_endian(const_cast<binary_header*>(this)->nr_groups, machine_endian, file_endian);
			map_convert_endian(const_cast<binary_header*>(this)->nr_time_steps, machine_endian, file_endian);
			map_convert_endian(const_cast<binary_header*>(this)->total_nr_chars_in_attribute_names, machine_endian, file_endian);
		}
		// write fixed length part of binary header
		bool success = 1 == fwrite(static_cast<const binary_header_lead*>(this), sizeof(binary_header_lead), 1, fp);

		if (machine_endian != file_endian) {
			map_convert_endian(const_cast<binary_header*>(this)->nr_points, file_endian, machine_endian);
			map_convert_endian(const_cast<binary_header*>(this)->nr_groups, file_endian, machine_endian);
			map_convert_endian(const_cast<binary_header*>(this)->nr_time_steps, file_endian, machine_endian);
			map_convert_endian(const_cast<binary_header*>(this)->total_nr_chars_in_attribute_names, file_endian, machine_endian);
		}
		if (!success) {
			fclose(fp);
			return false;
		}
		// write variable length part of header
		size_t cnt = fwrite(&concatenated_attribute_names[0], 1, total_nr_chars_in_attribute_names, fp);
		if (cnt == total_nr_chars_in_attribute_names) {
			if (write_vector(fp, name_lengths) &&
				write_vector(fp, times) &&
				write_vector(fp, time_step_start)) {

				// write optional stuff
				bool success = true;
				if ((format.flags & FF_COORDINATE_INTERVALS) != 0) {
					size_t coord_size = type_sizes[format.point_coord_type];
					if (!coordinate_intervals) {
						vec3 box[2] = { get_min_point(), get_max_point() };
						const_cast<binary_header*>(this)->set_coordinate_intervals(box[0](0), box[0](1), box[0](1), box[1](0), box[1](1), box[1](2));
					}
					map_convert_endian(coordinate_intervals, 6, machine_endian, file_endian, uint32_t(coord_size));
					success = fwrite(coordinate_intervals, coord_size, 6, fp) == 6;
					map_convert_endian(coordinate_intervals, 6, file_endian, machine_endian, uint32_t(coord_size));
				}

				if (success && ((format.flags & FF_ATTRIBUTE_RANGES) != 0)) {
					if (!write_vector(fp, attribute_ranges))
						success = false;
				}
				if (success) {
					// pass back file pointer or close file 
					if (fpp)
						*fpp = fp;
					else
						fclose(fp);

					return true;
				}
			}
		}
		fclose(fp);
		return false;
	}

	void binary_header::convert_vector_void(CoordinateType src_type, const void* src_ptr, CoordinateType dst_type, void* dst_ptr, size_t cnt, bool is_attr) const
	{
		if (is_attr && ((format.flags & FF_ATTRIBUTE_RANGES) != 0)) {
			if (dst_type == CT_FLT32) {
				switch (src_type) {
				case CT_UINT8:  convert_from_coordinate_vector(reinterpret_cast<const uint8_t*>(src_ptr), std::numeric_limits<uint8_t>::min(), std::numeric_limits<uint8_t>::max(), reinterpret_cast<float*>(dst_ptr), attribute_ranges, cnt); break;
				case CT_UINT16: convert_from_coordinate_vector(reinterpret_cast<const uint16_t*>(src_ptr), std::numeric_limits<uint16_t>::min(), std::numeric_limits<uint16_t>::max(), reinterpret_cast<float*>(dst_ptr), attribute_ranges, cnt); break;
				case CT_UINT32: convert_from_coordinate_vector(reinterpret_cast<const uint32_t*>(src_ptr), std::numeric_limits<uint32_t>::min(), std::numeric_limits<uint32_t>::max(), reinterpret_cast<float*>(dst_ptr), attribute_ranges, cnt); break;
				case CT_UINT64: convert_from_coordinate_vector(reinterpret_cast<const uint64_t*>(src_ptr), std::numeric_limits<uint64_t>::min(), std::numeric_limits<uint64_t>::max(), reinterpret_cast<float*>(dst_ptr), attribute_ranges, cnt); break;
				case CT_INT8:   convert_from_coordinate_vector(reinterpret_cast<const int8_t*>(src_ptr), std::numeric_limits<int8_t>::min(), std::numeric_limits<int8_t>::max(), reinterpret_cast<float*>(dst_ptr), attribute_ranges, cnt); break;
				case CT_INT16:  convert_from_coordinate_vector(reinterpret_cast<const int16_t*>(src_ptr), std::numeric_limits<int16_t>::min(), std::numeric_limits<int16_t>::max(), reinterpret_cast<float*>(dst_ptr), attribute_ranges, cnt); break;
				case CT_INT32:  convert_from_coordinate_vector(reinterpret_cast<const int32_t*>(src_ptr), std::numeric_limits<int32_t>::min(), std::numeric_limits<int32_t>::max(), reinterpret_cast<float*>(dst_ptr), attribute_ranges, cnt); break;
				case CT_INT64:  convert_from_coordinate_vector(reinterpret_cast<const int64_t*>(src_ptr), std::numeric_limits<int64_t>::min(), std::numeric_limits<int64_t>::max(), reinterpret_cast<float*>(dst_ptr), attribute_ranges, cnt); break;
				case CT_FLT32:  convert_from_coordinate_vector(reinterpret_cast<const float*>(src_ptr), 0.0f, 1.0f, reinterpret_cast<float*>(dst_ptr), attribute_ranges, cnt); break;
				case CT_FLT64:  convert_from_coordinate_vector(reinterpret_cast<const double*>(src_ptr), 0.0, 1.0, reinterpret_cast<float*>(dst_ptr), attribute_ranges, cnt); break;
				}
				return;
			}
			if (src_type == CT_FLT32) {
				switch (dst_type) {
				case CT_UINT8:  convert_to_coordinate_vector(reinterpret_cast<uint8_t*> (dst_ptr), std::numeric_limits<uint8_t>::min(), std::numeric_limits<uint8_t>::max(),   reinterpret_cast<const float*>(src_ptr), attribute_ranges, cnt); break;
				case CT_UINT16: convert_to_coordinate_vector(reinterpret_cast<uint16_t*>(dst_ptr), std::numeric_limits<uint16_t>::min(), std::numeric_limits<uint16_t>::max(), reinterpret_cast<const float*>(src_ptr), attribute_ranges, cnt); break;
				case CT_UINT32: convert_to_coordinate_vector(reinterpret_cast<uint32_t*>(dst_ptr), std::numeric_limits<uint32_t>::min(), std::numeric_limits<uint32_t>::max(), reinterpret_cast<const float*>(src_ptr), attribute_ranges, cnt); break;
				case CT_UINT64: convert_to_coordinate_vector(reinterpret_cast<uint64_t*>(dst_ptr), std::numeric_limits<uint64_t>::min(), std::numeric_limits<uint64_t>::max(), reinterpret_cast<const float*>(src_ptr), attribute_ranges, cnt); break;
				case CT_INT8:   convert_to_coordinate_vector(reinterpret_cast<int8_t*>  (dst_ptr), std::numeric_limits<int8_t>::min(), std::numeric_limits<int8_t>::max(),     reinterpret_cast<const float*>(src_ptr), attribute_ranges, cnt); break;
				case CT_INT16:  convert_to_coordinate_vector(reinterpret_cast<int16_t*> (dst_ptr), std::numeric_limits<int16_t>::min(), std::numeric_limits<int16_t>::max(),   reinterpret_cast<const float*>(src_ptr), attribute_ranges, cnt); break;
				case CT_INT32:  convert_to_coordinate_vector(reinterpret_cast<int32_t*> (dst_ptr), std::numeric_limits<int32_t>::min(), std::numeric_limits<int32_t>::max(),   reinterpret_cast<const float*>(src_ptr), attribute_ranges, cnt); break;
				case CT_INT64:  convert_to_coordinate_vector(reinterpret_cast<int64_t*> (dst_ptr), std::numeric_limits<int64_t>::min(), std::numeric_limits<int64_t>::max(),   reinterpret_cast<const float*>(src_ptr), attribute_ranges, cnt); break;
				case CT_FLT32:  convert_to_coordinate_vector(reinterpret_cast<float*>   (dst_ptr), 0.0f, 1.0f,                                                                 reinterpret_cast<const float*>(src_ptr), attribute_ranges, cnt); break;
				case CT_FLT64:  convert_to_coordinate_vector(reinterpret_cast<double*>  (dst_ptr), 0.0, 1.0,                                                                   reinterpret_cast<const float*>(src_ptr), attribute_ranges, cnt); break;
				}
				return;
			}
		}
		switch (src_type) {
		case CT_UINT8  :
			switch (dst_type) {
			case CT_UINT16: convert_coordinate_vector(reinterpret_cast<const uint8_t*>(src_ptr), reinterpret_cast<uint16_t*>(dst_ptr), cnt); break;
			case CT_UINT32: convert_coordinate_vector(reinterpret_cast<const uint8_t*>(src_ptr), reinterpret_cast<uint32_t*>(dst_ptr), cnt); break;
			case CT_UINT64: convert_coordinate_vector(reinterpret_cast<const uint8_t*>(src_ptr), reinterpret_cast<uint64_t*>(dst_ptr), cnt); break;
			case CT_INT8:   convert_coordinate_vector(reinterpret_cast<const uint8_t*>(src_ptr), reinterpret_cast<int8_t*>(dst_ptr), cnt); break;
			case CT_INT16:  convert_coordinate_vector(reinterpret_cast<const uint8_t*>(src_ptr), reinterpret_cast<int16_t*>(dst_ptr), cnt); break;
			case CT_INT32:  convert_coordinate_vector(reinterpret_cast<const uint8_t*>(src_ptr), reinterpret_cast<int32_t*>(dst_ptr), cnt); break;
			case CT_INT64:  convert_coordinate_vector(reinterpret_cast<const uint8_t*>(src_ptr), reinterpret_cast<int64_t*>(dst_ptr), cnt); break;
			case CT_FLT32:  convert_coordinate_vector(reinterpret_cast<const uint8_t*>(src_ptr), reinterpret_cast<float*>(dst_ptr), cnt); break;
			case CT_FLT64:  convert_coordinate_vector(reinterpret_cast<const uint8_t*>(src_ptr), reinterpret_cast<double*>(dst_ptr), cnt); break;
			} break;
		case CT_UINT16 :
			switch (dst_type) {
			case CT_UINT8:  convert_coordinate_vector(reinterpret_cast<const uint16_t*>(src_ptr), reinterpret_cast<uint8_t*>(dst_ptr), cnt); break;
			case CT_UINT32: convert_coordinate_vector(reinterpret_cast<const uint16_t*>(src_ptr), reinterpret_cast<uint32_t*>(dst_ptr), cnt); break;
			case CT_UINT64: convert_coordinate_vector(reinterpret_cast<const uint16_t*>(src_ptr), reinterpret_cast<uint64_t*>(dst_ptr), cnt); break;
			case CT_INT8:   convert_coordinate_vector(reinterpret_cast<const uint16_t*>(src_ptr), reinterpret_cast<int8_t*>(dst_ptr), cnt); break;
			case CT_INT16:  convert_coordinate_vector(reinterpret_cast<const uint16_t*>(src_ptr), reinterpret_cast<int16_t*>(dst_ptr), cnt); break;
			case CT_INT32:  convert_coordinate_vector(reinterpret_cast<const uint16_t*>(src_ptr), reinterpret_cast<int32_t*>(dst_ptr), cnt); break;
			case CT_INT64:  convert_coordinate_vector(reinterpret_cast<const uint16_t*>(src_ptr), reinterpret_cast<int64_t*>(dst_ptr), cnt); break;
			case CT_FLT32:  convert_coordinate_vector(reinterpret_cast<const uint16_t*>(src_ptr), reinterpret_cast<float*>(dst_ptr), cnt); break;
			case CT_FLT64:  convert_coordinate_vector(reinterpret_cast<const uint16_t*>(src_ptr), reinterpret_cast<double*>(dst_ptr), cnt); break;
			} break;
		case CT_UINT32 :
			switch (dst_type) {
			case CT_UINT8:  convert_coordinate_vector(reinterpret_cast<const uint32_t*>(src_ptr), reinterpret_cast<uint8_t*>(dst_ptr), cnt); break;
			case CT_UINT16: convert_coordinate_vector(reinterpret_cast<const uint32_t*>(src_ptr), reinterpret_cast<uint16_t*>(dst_ptr), cnt); break;
			case CT_UINT64: convert_coordinate_vector(reinterpret_cast<const uint32_t*>(src_ptr), reinterpret_cast<uint64_t*>(dst_ptr), cnt); break;
			case CT_INT8:   convert_coordinate_vector(reinterpret_cast<const uint32_t*>(src_ptr), reinterpret_cast<int8_t*>(dst_ptr), cnt); break;
			case CT_INT16:  convert_coordinate_vector(reinterpret_cast<const uint32_t*>(src_ptr), reinterpret_cast<int16_t*>(dst_ptr), cnt); break;
			case CT_INT32:  convert_coordinate_vector(reinterpret_cast<const uint32_t*>(src_ptr), reinterpret_cast<int32_t*>(dst_ptr), cnt); break;
			case CT_INT64:  convert_coordinate_vector(reinterpret_cast<const uint32_t*>(src_ptr), reinterpret_cast<int64_t*>(dst_ptr), cnt); break;
			case CT_FLT32:  convert_coordinate_vector(reinterpret_cast<const uint32_t*>(src_ptr), reinterpret_cast<float*>(dst_ptr), cnt); break;
			case CT_FLT64:  convert_coordinate_vector(reinterpret_cast<const uint32_t*>(src_ptr), reinterpret_cast<double*>(dst_ptr), cnt); break;
			} break;
		case CT_UINT64 :
			switch (dst_type) {
			case CT_UINT8:  convert_coordinate_vector(reinterpret_cast<const uint64_t*>(src_ptr), reinterpret_cast<uint8_t*>(dst_ptr), cnt); break;
			case CT_UINT16: convert_coordinate_vector(reinterpret_cast<const uint64_t*>(src_ptr), reinterpret_cast<uint16_t*>(dst_ptr), cnt); break;
			case CT_UINT32: convert_coordinate_vector(reinterpret_cast<const uint64_t*>(src_ptr), reinterpret_cast<uint32_t*>(dst_ptr), cnt); break;
			case CT_INT8:   convert_coordinate_vector(reinterpret_cast<const uint64_t*>(src_ptr), reinterpret_cast<int8_t*>(dst_ptr), cnt); break;
			case CT_INT16:  convert_coordinate_vector(reinterpret_cast<const uint64_t*>(src_ptr), reinterpret_cast<int16_t*>(dst_ptr), cnt); break;
			case CT_INT32:  convert_coordinate_vector(reinterpret_cast<const uint64_t*>(src_ptr), reinterpret_cast<int32_t*>(dst_ptr), cnt); break;
			case CT_INT64:  convert_coordinate_vector(reinterpret_cast<const uint64_t*>(src_ptr), reinterpret_cast<int64_t*>(dst_ptr), cnt); break;
			case CT_FLT32:  convert_coordinate_vector(reinterpret_cast<const uint64_t*>(src_ptr), reinterpret_cast<float*>(dst_ptr), cnt); break;
			case CT_FLT64:  convert_coordinate_vector(reinterpret_cast<const uint64_t*>(src_ptr), reinterpret_cast<double*>(dst_ptr), cnt); break;
			} break;
		case CT_INT8   :
			switch (dst_type) {
			case CT_UINT8:  convert_coordinate_vector(reinterpret_cast<const int8_t*>(src_ptr), reinterpret_cast<uint8_t*>(dst_ptr), cnt); break;
			case CT_UINT16: convert_coordinate_vector(reinterpret_cast<const int8_t*>(src_ptr), reinterpret_cast<uint16_t*>(dst_ptr), cnt); break;
			case CT_UINT32: convert_coordinate_vector(reinterpret_cast<const int8_t*>(src_ptr), reinterpret_cast<uint32_t*>(dst_ptr), cnt); break;
			case CT_UINT64: convert_coordinate_vector(reinterpret_cast<const int8_t*>(src_ptr), reinterpret_cast<uint64_t*>(dst_ptr), cnt); break;
			case CT_INT16:  convert_coordinate_vector(reinterpret_cast<const int8_t*>(src_ptr), reinterpret_cast<int16_t*>(dst_ptr), cnt); break;
			case CT_INT32:  convert_coordinate_vector(reinterpret_cast<const int8_t*>(src_ptr), reinterpret_cast<int32_t*>(dst_ptr), cnt); break;
			case CT_INT64:  convert_coordinate_vector(reinterpret_cast<const int8_t*>(src_ptr), reinterpret_cast<int64_t*>(dst_ptr), cnt); break;
			case CT_FLT32:  convert_coordinate_vector(reinterpret_cast<const int8_t*>(src_ptr), reinterpret_cast<float*>(dst_ptr), cnt); break;
			case CT_FLT64:  convert_coordinate_vector(reinterpret_cast<const int8_t*>(src_ptr), reinterpret_cast<double*>(dst_ptr), cnt); break;
			} break;
		case CT_INT16  :
			switch (dst_type) {
			case CT_UINT8:  convert_coordinate_vector(reinterpret_cast<const int16_t*>(src_ptr), reinterpret_cast<uint8_t*>(dst_ptr), cnt); break;
			case CT_UINT16: convert_coordinate_vector(reinterpret_cast<const int16_t*>(src_ptr), reinterpret_cast<uint16_t*>(dst_ptr), cnt); break;
			case CT_UINT32: convert_coordinate_vector(reinterpret_cast<const int16_t*>(src_ptr), reinterpret_cast<uint32_t*>(dst_ptr), cnt); break;
			case CT_UINT64: convert_coordinate_vector(reinterpret_cast<const int16_t*>(src_ptr), reinterpret_cast<uint64_t*>(dst_ptr), cnt); break;
			case CT_INT8:   convert_coordinate_vector(reinterpret_cast<const int16_t*>(src_ptr), reinterpret_cast<int8_t*>(dst_ptr), cnt); break;
			case CT_INT32:  convert_coordinate_vector(reinterpret_cast<const int16_t*>(src_ptr), reinterpret_cast<int32_t*>(dst_ptr), cnt); break;
			case CT_INT64:  convert_coordinate_vector(reinterpret_cast<const int16_t*>(src_ptr), reinterpret_cast<int64_t*>(dst_ptr), cnt); break;
			case CT_FLT32:  convert_coordinate_vector(reinterpret_cast<const int16_t*>(src_ptr), reinterpret_cast<float*>(dst_ptr), cnt); break;
			case CT_FLT64:  convert_coordinate_vector(reinterpret_cast<const int16_t*>(src_ptr), reinterpret_cast<double*>(dst_ptr), cnt); break;
			} break;
		case CT_INT32  :
			switch (dst_type) {
			case CT_UINT8:  convert_coordinate_vector(reinterpret_cast<const int32_t*>(src_ptr), reinterpret_cast<uint8_t*>(dst_ptr), cnt); break;
			case CT_UINT16: convert_coordinate_vector(reinterpret_cast<const int32_t*>(src_ptr), reinterpret_cast<uint16_t*>(dst_ptr), cnt); break;
			case CT_UINT32: convert_coordinate_vector(reinterpret_cast<const int32_t*>(src_ptr), reinterpret_cast<uint32_t*>(dst_ptr), cnt); break;
			case CT_UINT64: convert_coordinate_vector(reinterpret_cast<const int32_t*>(src_ptr), reinterpret_cast<uint64_t*>(dst_ptr), cnt); break;
			case CT_INT8:   convert_coordinate_vector(reinterpret_cast<const int32_t*>(src_ptr), reinterpret_cast<int8_t*>(dst_ptr), cnt); break;
			case CT_INT16:  convert_coordinate_vector(reinterpret_cast<const int32_t*>(src_ptr), reinterpret_cast<int16_t*>(dst_ptr), cnt); break;
			case CT_INT64:  convert_coordinate_vector(reinterpret_cast<const int32_t*>(src_ptr), reinterpret_cast<int64_t*>(dst_ptr), cnt); break;
			case CT_FLT32:  convert_coordinate_vector(reinterpret_cast<const int32_t*>(src_ptr), reinterpret_cast<float*>(dst_ptr), cnt); break;
			case CT_FLT64:  convert_coordinate_vector(reinterpret_cast<const int32_t*>(src_ptr), reinterpret_cast<double*>(dst_ptr), cnt); break;
			} break;
		case CT_INT64  :
			switch (dst_type) {
			case CT_UINT8:  convert_coordinate_vector(reinterpret_cast<const int64_t*>(src_ptr), reinterpret_cast<uint8_t*>(dst_ptr), cnt); break;
			case CT_UINT16: convert_coordinate_vector(reinterpret_cast<const int64_t*>(src_ptr), reinterpret_cast<uint16_t*>(dst_ptr), cnt); break;
			case CT_UINT32: convert_coordinate_vector(reinterpret_cast<const int64_t*>(src_ptr), reinterpret_cast<uint32_t*>(dst_ptr), cnt); break;
			case CT_UINT64: convert_coordinate_vector(reinterpret_cast<const int64_t*>(src_ptr), reinterpret_cast<uint64_t*>(dst_ptr), cnt); break;
			case CT_INT8:   convert_coordinate_vector(reinterpret_cast<const int64_t*>(src_ptr), reinterpret_cast<int8_t*>(dst_ptr), cnt); break;
			case CT_INT16:  convert_coordinate_vector(reinterpret_cast<const int64_t*>(src_ptr), reinterpret_cast<int16_t*>(dst_ptr), cnt); break;
			case CT_INT32:  convert_coordinate_vector(reinterpret_cast<const int64_t*>(src_ptr), reinterpret_cast<int32_t*>(dst_ptr), cnt); break;
			case CT_FLT32:  convert_coordinate_vector(reinterpret_cast<const int64_t*>(src_ptr), reinterpret_cast<float*>(dst_ptr), cnt); break;
			case CT_FLT64:  convert_coordinate_vector(reinterpret_cast<const int64_t*>(src_ptr), reinterpret_cast<double*>(dst_ptr), cnt); break;
			} break;
		case CT_FLT32  :
			switch (dst_type) {
			case CT_UINT8:  convert_coordinate_vector(reinterpret_cast<const float*>(src_ptr), reinterpret_cast<uint8_t*>(dst_ptr), cnt); break;
			case CT_UINT16: convert_coordinate_vector(reinterpret_cast<const float*>(src_ptr), reinterpret_cast<uint16_t*>(dst_ptr), cnt); break;
			case CT_UINT32: convert_coordinate_vector(reinterpret_cast<const float*>(src_ptr), reinterpret_cast<uint32_t*>(dst_ptr), cnt); break;
			case CT_UINT64: convert_coordinate_vector(reinterpret_cast<const float*>(src_ptr), reinterpret_cast<uint64_t*>(dst_ptr), cnt); break;
			case CT_INT8:   convert_coordinate_vector(reinterpret_cast<const float*>(src_ptr), reinterpret_cast<int8_t*>(dst_ptr), cnt); break;
			case CT_INT16:  convert_coordinate_vector(reinterpret_cast<const float*>(src_ptr), reinterpret_cast<int16_t*>(dst_ptr), cnt); break;
			case CT_INT32:  convert_coordinate_vector(reinterpret_cast<const float*>(src_ptr), reinterpret_cast<int32_t*>(dst_ptr), cnt); break;
			case CT_INT64:  convert_coordinate_vector(reinterpret_cast<const float*>(src_ptr), reinterpret_cast<int64_t*>(dst_ptr), cnt); break;
			case CT_FLT64:  convert_coordinate_vector(reinterpret_cast<const float*>(src_ptr), reinterpret_cast<double*>(dst_ptr), cnt); break;
			} break;
		case CT_FLT64  :
			switch (dst_type) {
			case CT_UINT8:  convert_coordinate_vector(reinterpret_cast<const double*>(src_ptr), reinterpret_cast<uint8_t*>(dst_ptr), cnt); break;
			case CT_UINT16: convert_coordinate_vector(reinterpret_cast<const double*>(src_ptr), reinterpret_cast<uint16_t*>(dst_ptr), cnt); break;
			case CT_UINT32: convert_coordinate_vector(reinterpret_cast<const double*>(src_ptr), reinterpret_cast<uint32_t*>(dst_ptr), cnt); break;
			case CT_UINT64: convert_coordinate_vector(reinterpret_cast<const double*>(src_ptr), reinterpret_cast<uint64_t*>(dst_ptr), cnt); break;
			case CT_INT8:   convert_coordinate_vector(reinterpret_cast<const double*>(src_ptr), reinterpret_cast<int8_t*>(dst_ptr), cnt); break;
			case CT_INT16:  convert_coordinate_vector(reinterpret_cast<const double*>(src_ptr), reinterpret_cast<int16_t*>(dst_ptr), cnt); break;
			case CT_INT32:  convert_coordinate_vector(reinterpret_cast<const double*>(src_ptr), reinterpret_cast<int32_t*>(dst_ptr), cnt); break;
			case CT_INT64:  convert_coordinate_vector(reinterpret_cast<const double*>(src_ptr), reinterpret_cast<int64_t*>(dst_ptr), cnt); break;
			case CT_FLT32:  convert_coordinate_vector(reinterpret_cast<const double*>(src_ptr), reinterpret_cast<float*>(dst_ptr), cnt); break;
			} break;
		}
	}
	bool binary_file::read_variant_vector_void(FILE* fp, void* values, size_t cnt, CoordinateType file_type, CoordinateType value_type, bool is_attr) const
	{
		size_t file_size  = type_sizes[file_type];
		size_t value_size = type_sizes[value_type];
		Endian file = Endian(format.endian);
		Endian machine = get_endian();

		// if size of coordinate types differ, construct temporary vector
		if (file_size != value_size) {
			size_t nr_bytes = cnt * file_size;
			std::vector<uint8_t> temp(nr_bytes);
			if (fread(&temp.front(), 1, nr_bytes, fp) != nr_bytes)
				return false;
			map_convert_endian(&temp.front(), cnt, file, machine, uint32_t(file_size));
			convert_vector_void(file_type, &temp.front(), value_type, values, cnt, is_attr);
		}
		// otherwise read into target vector and convert in place
		else {
			if (fread(&values, file_size, cnt, fp) != cnt)
				return false;
			map_convert_endian(values, cnt, file, machine, uint32_t(file_size));
			if (file_type != value_type)
				convert_vector_void(file_type, values, value_type, values, cnt, is_attr);
		}
		return true;
	}
	bool binary_file::write_variant_vector_void(FILE* fp, const void* values, size_t cnt, CoordinateType file_type, CoordinateType value_type, bool is_attr) const
	{
		size_t file_size = type_sizes[file_type];
		size_t value_size = type_sizes[value_type];
		Endian file = Endian(format.endian);
		Endian machine = get_endian();

		// if types of file and value differ, construct temporary vector
		if (file_type != value_type) {
			size_t nr_bytes = cnt * file_size;
			std::vector<uint8_t> temp(nr_bytes);
			convert_vector_void(value_type, values, file_type, &temp.front(), cnt, is_attr);
			map_convert_endian(&temp.front(), cnt, machine, file, uint32_t(file_size));
			if (fwrite(&temp.front(), 1, nr_bytes, fp) != nr_bytes)
				return false;
		}
		// otherwise read into target vector and convert in place
		else {
			map_convert_endian(const_cast<void*>(values), cnt, file, machine, uint32_t(file_size));
			bool success = fwrite(&values, file_size, cnt, fp) == cnt;
			map_convert_endian(const_cast<void*>(values), cnt, file, machine, uint32_t(file_size));
			if (!success)
				return false;
		}
		return true;
	}

	bool binary_file::read_time_step_void(
		const std::string& file_name, uint64_t beg, uint64_t cnt,
		void* pnt_ptr, CoordinateType pnt_type,
		void* grp_ptr, CoordinateType grp_type,
		void* att_ptr, CoordinateType att_type) const
	{
		uint64_t offset = 0;
		FILE* fp = 0;
		if ((format.flags & FF_SEPARATE_FRAME_FILE) != 0)
			fp = fopen((file_name + ".caf").c_str(), "rb");
		else {
			fp = fopen((file_name + ".cae").c_str(), "rb");
			offset = get_header_size();
		}
		if (!fp)
			return false;

		offset += beg * get_entry_size();

		bool success = true;
		if (fseek(fp, long(offset), SEEK_SET) != 0) 
			success = false;
		else {
			if (!read_variant_vector_void(fp, pnt_ptr, size_t(3 * cnt), CoordinateType(format.point_coord_type), pnt_type) ||
				!read_variant_vector_void(fp, grp_ptr, size_t(cnt), CoordinateType(format.group_index_type), grp_type) ||
				!read_variant_vector_void(fp, att_ptr, size_t(nr_attributes*cnt), CoordinateType(format.attribute_type), att_type, true))
				success = false;
		}
		fclose(fp);
		return success;
	}
	bool binary_file::write_time_step_void(const std::string& file_name, uint64_t cnt,
		const void* pnt_ptr, CoordinateType pnt_type,
		const void* grp_ptr, CoordinateType grp_type,
		const void* att_ptr, CoordinateType att_type, bool write_hdr) const
	{
		std::cout << "append time step to " << file_name << " with " << cnt << " items" << std::endl;

		FILE* fp = 0;
		if ((format.flags & FF_SEPARATE_FRAME_FILE) != 0)
			fp = fopen((file_name + ".caf").c_str(), "ab");
		else {
			fp = fopen((file_name + ".cae").c_str(), "ab");
		}
		if (!fp)
			return false;

		bool success = true;
		if (!write_variant_vector_void(fp, pnt_ptr, size_t(3 * cnt), CoordinateType(format.point_coord_type), pnt_type) ||
			!write_variant_vector_void(fp, grp_ptr, size_t(cnt), CoordinateType(format.group_index_type), grp_type) ||
			!write_variant_vector_void(fp, att_ptr, size_t(nr_attributes*cnt), CoordinateType(format.attribute_type), att_type, true))
			success = false;
		fclose(fp);
		if (write_hdr)
			return write_header(file_name);
		return success;
	}
}

