#include "control_sphere_renderer.h"
#include <cgv_gl/gl/gl.h>
#include <cgv_gl/gl/gl_tools.h>

control_sphere_renderer& ref_control_sphere_renderer(cgv::render::context& ctx, int ref_count_change)
{
	static int ref_count = 0;
	static control_sphere_renderer r;
	r.manage_singleton(ctx, "control_sphere_renderer", ref_count, ref_count_change);
	return r;
}

control_sphere_render_style::control_sphere_render_style()
{
	use_visibility = false;
}

control_sphere_renderer::control_sphere_renderer()
{
	has_visibility_indices = false;
	has_visibilities = false;
}
bool control_sphere_renderer::build_shader_program(cgv::render::context& ctx, cgv::render::shader_program& prog, const cgv::render::shader_define_map& defines)
{
	return prog.build_program(ctx, "control_sphere.glpr", true, defines);
}
bool control_sphere_renderer::validate_attributes(const cgv::render::context& ctx) const
{
	bool res = surface_renderer::validate_attributes(ctx);
	if (!res)
		return false;
	const control_sphere_render_style& srs = get_style<control_sphere_render_style>();
	if (!has_visibilities && srs.use_visibility) {
		ctx.error("control_sphere_renderer::validate_attributes() visibilities not set");
		res = false;
	}
	return res;
}
bool control_sphere_renderer::enable(cgv::render::context& ctx)
{
	bool res = sphere_renderer::enable(ctx);
	const control_sphere_render_style& srs = get_style<control_sphere_render_style>();
	if (ref_prog().is_linked()) {
		// visibility
		ref_prog().set_uniform(ctx, "use_visibility", srs.use_visibility);
	}
	else
		res = false;
	return res;
}
void control_sphere_renderer::set_visibilities_index_array(const cgv::render::context& ctx, const std::vector<int>& group_indices)
{
	has_visibility_indices = true;
	set_attribute_array(ctx, "visibility_index", group_indices);
}
void control_sphere_renderer::set_visibilities_index_array(const cgv::render::context& ctx, const int* group_indices, size_t nr_elements)
{
	has_visibility_indices = true;
	set_attribute_array(ctx, "visibility_index", group_indices, nr_elements, 0);
}
void control_sphere_renderer::set_visibilities_index_array(const cgv::render::context& ctx, cgv::render::type_descriptor element_type, const cgv::render::vertex_buffer& vbo, size_t offset_in_bytes, size_t nr_elements, unsigned stride_in_bytes)
{
	has_visibility_indices = true;
	set_attribute_array(ctx, "visibility_index", element_type, vbo, offset_in_bytes, nr_elements, stride_in_bytes);
}
