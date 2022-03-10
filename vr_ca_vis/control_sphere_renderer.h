#pragma once

#include <cgv_gl/sphere_renderer.h>

class control_sphere_renderer;

//! reference to a singleton control sphere renderer that can be shared among drawables
/*! the second parameter is used for reference counting. Use +1 in your init method,
-1 in your clear method and default 0 argument otherwise. If internal reference
counter decreases to 0, singleton renderer is destructed. */
extern control_sphere_renderer& ref_control_sphere_renderer(cgv::render::context& ctx, int ref_count_change = 0);

/** render style for sphere rendere */
struct control_sphere_render_style : public cgv::render::sphere_render_style
{
	/// whether to use visibility indexed through index, defaults to false
	bool use_visibility;
	/// construct with default values
	control_sphere_render_style();
};

/// renderer that supports splatting of spheres
class control_sphere_renderer : public cgv::render::sphere_renderer
{
protected:
	bool has_visibility_indices;
	bool has_visibilities;

	/// build sphere program
	bool build_shader_program(cgv::render::context& ctx, cgv::render::shader_program& prog, const cgv::render::shader_define_map& defines);
public:
	///
	control_sphere_renderer();
	///
	bool validate_attributes(const cgv::render::context& ctx) const;
	///
	bool enable(cgv::render::context& ctx);

	/// method to set the group visibility index attribute
	void set_visibilities_index_array(const cgv::render::context& ctx, const std::vector<int>& group_indices);
	/// method to set the group visibility index attribute
	void set_visibilities_index_array(const cgv::render::context& ctx, const int* group_indices, size_t nr_elements);
	/// method to set the group visibility index attribute from a vertex buffer object, the element type must be given as explicit template parameter
	void set_visibilities_index_array(const cgv::render::context& ctx, cgv::render::type_descriptor element_type, const cgv::render::vertex_buffer& vbo, size_t offset_in_bytes, size_t nr_elements, unsigned stride_in_bytes = 0);
	/// template method to set the group index color attribute from a vertex buffer object, the element type must be given as explicit template parameter
	template <typename T>
	void set_visibilities_index_array(const cgv::render::context& ctx, const cgv::render::vertex_buffer& vbo, size_t offset_in_bytes, size_t nr_elements, unsigned stride_in_bytes = 0) { set_visibilities_index_array(ctx, cgv::render::type_descriptor(cgv::render::element_descriptor_traits<T>::get_type_descriptor(T()), true), vbo, offset_in_bytes, nr_elements, stride_in_bytes); }
	template <typename T>
	void set_visibilities(const cgv::render::context& ctx, const std::vector<T>& visibilities) { has_visibilities = true; ref_prog().set_uniform_array(ctx, "visibilities", visibilities); }
	template <typename T>
	void set_visibilities(const cgv::render::context& ctx, const T* visibilities, size_t nr_elements) { has_visibilities = true; ref_prog().set_uniform_array(ctx, "visibilities", visibilities, nr_elements); }
};
