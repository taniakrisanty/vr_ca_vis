#include "clipped_box_renderer.h"
#include <cgv_gl/gl/gl.h>
#include <cgv_gl/gl/gl_tools.h>

clipped_box_renderer& ref_clipped_box_renderer(cgv::render::context& ctx, int ref_count_change)
{
	static int ref_count = 0;
	static clipped_box_renderer r;
	r.manage_singleton(ctx, "clipped_box_renderer", ref_count, ref_count_change);
	return r;
}
clipped_box_renderer::clipped_box_renderer()
{
	has_extents = false;
	position_is_center = true;
	has_translations = false;
	has_rotations = false;

	num_clipping_planes = 0;
}
void clipped_box_renderer::set_clipping_planes(const std::vector<vec4>& _clipping_planes)
{
	num_clipping_planes = _clipping_planes.size() > MAX_CLIPPING_PLANES ? MAX_CLIPPING_PLANES : _clipping_planes.size();
	std::copy(_clipping_planes.begin(), _clipping_planes.begin() + num_clipping_planes, clipping_planes);
}
/// build box program
bool clipped_box_renderer::build_shader_program(cgv::render::context& ctx, cgv::render::shader_program& prog, const cgv::render::shader_define_map& defines)
{
	return prog.build_program(ctx, "clipped_box.glpr", true, defines);
}
/// 
bool clipped_box_renderer::enable(cgv::render::context& ctx)
{
	box_renderer::enable(ctx);
	ref_prog().set_uniform(ctx, "num_clipping_planes", num_clipping_planes);
	ref_prog().set_uniform_array(ctx, "clipping_planes", clipping_planes, MAX_CLIPPING_PLANES);
	return true;
}
