#pragma once

#include <cgv_gl/box_renderer.h>

class clipped_box_renderer;

//! reference to a singleton box renderer that is shared among drawables
/*! the second parameter is used for reference counting. Use +1 in your init method,
-1 in your clear method and default 0 argument otherwise. If internal reference
counter decreases to 0, singleton renderer is destructed. */
extern clipped_box_renderer& ref_clipped_box_renderer(cgv::render::context& ctx, int ref_count_change = 0);

/// clipped boxes use surface render styles
struct clipped_box_render_style : public cgv::render::box_render_style
{
	/// whether to use visibility indexed through index, defaults to false
	bool use_visibility;
	/// set default values
	clipped_box_render_style();
};

/// renderer that supports point splatting
class clipped_box_renderer : public cgv::render::box_renderer
{
public:
	static const int MAX_CLIPPING_PLANES = 8;
protected:
	bool has_visibility_indices;
	bool has_visibilities;

	int num_clipping_planes;
	vec4 clipping_planes[MAX_CLIPPING_PLANES];

	/// build clipped_box program
	bool build_shader_program(cgv::render::context& ctx, cgv::render::shader_program& prog, const cgv::render::shader_define_map& defines);
public:
	/// initializes position_is_center to true 
	clipped_box_renderer();
	/// check additionally the group attributes
	bool validate_attributes(const cgv::render::context& ctx) const;
	/// overload to activate group style
	bool enable(cgv::render::context& ctx);

	/// method to set the group visibility index attribute
	void set_visibilities_index_array(const cgv::render::context& ctx, const std::vector<unsigned>& group_indices);
	/// method to set the group visibility index attribute
	void set_visibilities_index_array(const cgv::render::context & ctx, const unsigned* group_indices, size_t nr_elements);
	/// method to set the group visibility index attribute from a vertex buffer object, the element type must be given as explicit template parameter
	void set_visibilities_index_array(const cgv::render::context& ctx, cgv::render::type_descriptor element_type, const cgv::render::vertex_buffer& vbo, size_t offset_in_bytes, size_t nr_elements, unsigned stride_in_bytes = 0);
	/// template method to set the group index color attribute from a vertex buffer object, the element type must be given as explicit template parameter
	template <typename T>
	void set_visibilities_index_array(const cgv::render::context& ctx, const cgv::render::vertex_buffer& vbo, size_t offset_in_bytes, size_t nr_elements, unsigned stride_in_bytes = 0) { set_visibilities_index_array(ctx, cgv::render::type_descriptor(cgv::render::element_descriptor_traits<T>::get_type_descriptor(T()), true), vbo, offset_in_bytes, nr_elements, stride_in_bytes); }
	template <typename T>
	void set_visibilities(const cgv::render::context& ctx, const std::vector<T>& visibilities) { has_visibilities = true; ref_prog().set_uniform_array(ctx, "visibilities", visibilities); }
	template <typename T>
	void set_visibilities(const cgv::render::context& ctx, const T* visibilities, size_t nr_elements) { has_visibilities = true; ref_prog().set_uniform_array(ctx, "visibilities", visibilities, nr_elements); }

	void set_clipping_planes(const std::vector<vec4>& _clipping_planes);
};
