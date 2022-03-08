#pragma once

#include <cgv_gl/box_renderer.h>

class clipped_box_renderer;

//! reference to a singleton box renderer that is shared among drawables
/*! the second parameter is used for reference counting. Use +1 in your init method,
-1 in your clear method and default 0 argument otherwise. If internal reference
counter decreases to 0, singleton renderer is destructed. */
extern clipped_box_renderer& ref_clipped_box_renderer(cgv::render::context& ctx, int ref_count_change = 0);

/// renderer that supports point splatting
class clipped_box_renderer : public cgv::render::box_renderer
{
public:
	static const int MAX_CLIPPING_PLANES = 8;
protected:
	int num_clipping_planes;
	vec4 clipping_planes[MAX_CLIPPING_PLANES];

	bool has_group_visibilities;
public:
	/// initializes position_is_center to true 
	clipped_box_renderer();
	void set_clipping_planes(const std::vector<vec4>& _clipping_planes);

	bool build_shader_program(cgv::render::context& ctx, cgv::render::shader_program& prog, const cgv::render::shader_define_map& defines);
	bool enable(cgv::render::context& ctx);

	template <typename T>
	void set_group_visibilities(const cgv::render::context& ctx, const std::vector<T>& group_visibilities) { has_group_visibilities = true; ref_prog().set_uniform_array(ctx, "group_visibilities", group_visibilities); }
	/// template method to set the group rotation from a vector of quaternions of type T, which should have 4 components
	template <typename T>
	void set_group_visibilities(const cgv::render::context& ctx, const T* group_visibilities, size_t nr_elements) { has_group_rotations = true; ref_prog().set_uniform_array(ctx, "group_visibilities", group_visibilities, nr_elements); }
};
