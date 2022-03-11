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
clipped_box_render_style::clipped_box_render_style()
{
	use_visibility = false;
}
clipped_box_renderer::clipped_box_renderer()
{
	has_visibility_indices = false;
	has_visibilities = false;

	num_clipping_planes = 0;

	burn = false;
	burn_distance = 0;
}
bool clipped_box_renderer::validate_attributes(const cgv::render::context& ctx) const
{
	// validate set attributes
	bool res = box_renderer::validate_attributes(ctx);
	if (!res)
		return false;
	const clipped_box_render_style& brs = get_style<clipped_box_render_style>();
	if (!has_visibilities && brs.use_visibility) {
		ctx.error("clipped_box_renderer::validate_attributes() visibilities not set");
		res = false;
	}
	return res;
}
bool clipped_box_renderer::build_shader_program(cgv::render::context& ctx, cgv::render::shader_program& prog, const cgv::render::shader_define_map& defines)
{
	return prog.build_program(ctx, "clipped_box.glpr", true, defines);
}
bool clipped_box_renderer::enable(cgv::render::context & ctx)
{
	bool res = box_renderer::enable(ctx);
	const clipped_box_render_style& brs = get_style<clipped_box_render_style>();
	if (ref_prog().is_linked()) {
		// visibility
		ref_prog().set_uniform(ctx, "use_visibility", brs.use_visibility);
		// clipping planes
		ref_prog().set_uniform(ctx, "num_clipping_planes", num_clipping_planes);
		ref_prog().set_uniform_array(ctx, "clipping_planes", clipping_planes, MAX_CLIPPING_PLANES);
		// burn
		ref_prog().set_uniform(ctx, "burn", burn);
		ref_prog().set_uniform(ctx, "burn_center", burn_center);
		ref_prog().set_uniform(ctx, "burn_distance", burn_distance);
	}
	else
		res = false;
	return res;
}
void clipped_box_renderer::set_visibilities_index_array(const cgv::render::context& ctx, const std::vector<unsigned>& group_indices)
{
	has_visibility_indices = true;
	set_attribute_array(ctx, "visibility_index", group_indices);
}
void clipped_box_renderer::set_visibilities_index_array(const cgv::render::context& ctx, const unsigned* group_indices, size_t nr_elements)
{
	has_visibility_indices = true;
	set_attribute_array(ctx, "visibility_index", group_indices, nr_elements, 0);
}
void clipped_box_renderer::set_visibilities_index_array(const cgv::render::context& ctx, cgv::render::type_descriptor element_type, const cgv::render::vertex_buffer& vbo, size_t offset_in_bytes, size_t nr_elements, unsigned stride_in_bytes)
{
	has_visibility_indices = true;
	set_attribute_array(ctx, "visibility_index", element_type, vbo, offset_in_bytes, nr_elements, stride_in_bytes);
}
void clipped_box_renderer::set_clipping_planes(const std::vector<vec4>& _clipping_planes)
{
	num_clipping_planes = std::min(_clipping_planes.size(), MAX_CLIPPING_PLANES);
	std::copy(_clipping_planes.begin(), _clipping_planes.begin() + num_clipping_planes, clipping_planes);
}
void clipped_box_renderer::set_torch(bool _burn, const vec3& _burn_center, float _burn_distance)
{
	burn = _burn;
	burn_center = _burn_center;
	burn_distance = _burn_distance;
}

//#include <cgv/gui/provider.h>
//
//struct clipped_box_render_style_gui_creator : public cgv::gui::gui_creator
//{
//	/// attempt to create a gui and return whether this was successful
//	bool create(cgv::gui::provider * p, const std::string& label,
//		void* value_ptr, const std::string& value_type,
//		const std::string& gui_type, const std::string& options, bool*)
//	{
//		if (value_type != cgv::type::info::type_name<clipped_box_render_style>::get_name())
//			return false;
//		clipped_box_render_style* brs_ptr = reinterpret_cast<clipped_box_render_style*>(value_ptr);
//
//		cgv::base::base* b = dynamic_cast<cgv::base::base*>(p);
//		p->add_member_control(b, "default_extent.x", brs_ptr->default_extent[0], "value_slider", "min=0.001;max=1000;log=true;ticks=true");
//		p->add_member_control(b, "default_extent.y", brs_ptr->default_extent[1], "value_slider", "min=0.001;max=1000;log=true;ticks=true");
//		p->add_member_control(b, "default_extent.z", brs_ptr->default_extent[2], "value_slider", "min=0.001;max=1000;log=true;ticks=true");
//		p->add_member_control(b, "relative_anchor.x", brs_ptr->relative_anchor[0], "value_slider", "min=-1;max=1;ticks=true");
//		p->add_member_control(b, "relative_anchor.y", brs_ptr->relative_anchor[1], "value_slider", "min=-1;max=1;ticks=true");
//		p->add_member_control(b, "relative_anchor.z", brs_ptr->relative_anchor[2], "value_slider", "min=-1;max=1;ticks=true");
//		p->add_member_control(b, "rounding", brs_ptr->rounding, "toggle");
//		p->add_member_control(b, "default_radius", brs_ptr->default_radius, "value_slider", "min=0.0;max=10;step=0.0001;log=true;ticks=true");
//		p->add_gui("box_render_style", *static_cast<cgv::render::box_render_style*>(brs_ptr));
//		return true;
//	}
//};
//		
//cgv::gui::gui_creator_registration<clipped_box_render_style_gui_creator> clipped_box_rs_gc_reg("clipped_box_render_style_gui_creator");
