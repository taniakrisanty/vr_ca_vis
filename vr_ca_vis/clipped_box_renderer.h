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
protected:
	std::vector<vec4> clipping_planes;
public:
	/// initializes position_is_center to true 
	clipped_box_renderer();
	void set_clipping_planes(const std::vector<vec4>& _clipping_planes);

	bool build_shader_program(cgv::render::context& ctx, cgv::render::shader_program& prog, const cgv::render::shader_define_map& defines);
	bool enable(cgv::render::context& ctx);
};
