// This source code is adapted from simple_primitive_container in vr_lab_test plugin

#pragma once

#include <cgv/base/node.h>
#include <cgv/render/drawable.h>
#include <cg_nui/focusable.h>
#include <cg_nui/pointable.h>
#include <cg_nui/grabable.h>
#include <cgv/gui/provider.h>
#include <cgv_gl/sphere_renderer.h>
#include <cgv_gl/cone_renderer.h>
#include <cgv_glutil/color_map.h>

#include <unordered_map>

#include "cell_data.h"
#include "clipped_box_renderer.h"
#include "control_sphere_renderer.h"
#include "regular_grid.h"

class cells_container_listener
{
public:
	virtual void on_cell_pointed_at(size_t cell_index, size_t node_index, const cgv::render::render_types::rgb& color = cgv::render::render_types::rgb()) = 0;
};

class cells_container :
	public cgv::base::node,
	public cgv::render::drawable,
	public cgv::nui::focusable,
	//public cgv::nui::grabable,
	public cgv::nui::pointable,
	public cgv::gui::provider
{
	clipped_box_render_style brs;
	control_sphere_render_style csrs;
	cgv::render::sphere_render_style srs;
	cgv::render::cone_render_style crs;
	vec3 debug_point;
	vec3 query_point_at_grab, position_at_grab;
	vec3 hit_point_at_trigger, position_at_trigger;
	cgv::nui::focus_configuration original_config;
public:
	// different possible object states
	enum class state_enum {
		idle,
		close,
		pointed,
		grabbed,
		triggered
	};
protected:
	cells_container_listener* listener;

	// acceleration data structure
	regular_grid<cell> grid;

	// geometry of cubes with color
	vec3 extent;
	quat rotation;

	mat4 scale_matrix;
	// inv_scale_matrix is used to prevent expensive scaling of all cells
	mat4 inv_scale_matrix;

	// cell types (ct1, ct2, etc)
	std::unordered_map<std::string, cell_type> cell_types;

	// cells start offset set by time_step_start
	size_t cells_start, cells_end;
	const std::vector<cell>* cells = NULL;

	// color map
	std::vector<cgv::glutil::color_map> color_maps;
	std::vector<std::map<unsigned int, rgba>> color_points_maps;

	// per group information
	std::vector<rgba> default_colors;

	std::vector<rgba> group_colors;
	std::vector<bool> group_colors_overrides;

	std::vector<size_t> peeled_cell_indices;
	std::vector<size_t> peeled_node_indices;

	// visibility filter by id
	std::vector<int> visibilities;

	std::vector<int> show_all_checks;
	std::vector<int> hide_all_checks;

	std::vector<int> show_checks;

	bool cells_out_of_date = true;

	// vertex buffer
	size_t cells_count = 0;
	size_t nodes_count = 0;

	// nodes geometry
	cgv::render::vertex_buffer vb_node_indices;
	cgv::render::vertex_buffer vb_nodes;
	cgv::render::vertex_buffer vb_colors;
	
	// centers geometry
	cgv::render::vertex_buffer vb_center_indices;
	cgv::render::vertex_buffer vb_centers;

	// clipping planes that are used by clipped_box geometry shader
	// in the form of ax + by + cz + d = 0
	std::vector<vec4> clipping_planes;

	// torch that is used by clipped_box geometry shader
	bool burn;
	vec3 burn_center;
	float burn_distance;

	vec3 unscaled_burn_center;

	// hid with focus on object
	cgv::nui::hid_identifier hid_id;
	// index of focused primitive
	int prim_idx = -1;
	// assuming that size_t in this particular system is at least 32-bit 
	const unsigned int cell_sign_bit = 0x40000000; // sign bit is the second bit
	const unsigned int cell_bitwise_shift = 15;
	const unsigned int cell_bitwise_and = 0x7FFF;
	// state of object
	state_enum state = state_enum::idle;
	/// return color modified based on state
	rgb get_modified_color(const rgb& color) const;

public:
	cells_container(cells_container_listener* _listener, const std::string& _name, const vec3& _extent = vec3(1.f), const quat& _rotation = quat(1, 0, 0, 0));
	/// return type name
	std::string get_type_name() const;
	/// callback on member updates to keep data structure consistent
	void on_set(void* member_ptr);
	//@name cgv::nui::focusable interface
	//@{
	bool focus_change(cgv::nui::focus_change_action action, cgv::nui::refocus_action rfa, const cgv::nui::focus_demand& demand, const cgv::gui::event& e, const cgv::nui::dispatch_info& dis_info);
	void stream_help(std::ostream& os);
	bool handle(const cgv::gui::event& e, const cgv::nui::dispatch_info& dis_info, cgv::nui::focus_request& request);
	//@}

	/// implement closest point algorithm and return whether this was found (failure only for invisible objects) and in this case set \c prj_point to closest point and \c prj_normal to corresponding surface normal
	//bool compute_closest_point(const vec3& point, vec3& prj_point, vec3& prj_normal, size_t& primitive_idx);
	/// implement ray object intersection and return whether intersection was found and in this case set \c hit_param to ray parameter and optionally \c hit_normal to surface normal of intersection
	bool compute_intersection(const vec3& ray_start, const vec3& ray_direction, float& hit_param, vec3& hit_normal, size_t& primitive_idx);

	//@name cgv::render::drawable interface
	//@{
	/// initialization called once per context creation
	bool init(cgv::render::context& ctx);
	/// called before context destruction to clean up GPU objects
	void clear(cgv::render::context& ctx);
	/// 
	void init_frame(cgv::render::context& ctx);
	/// draw scene here
	void draw(cgv::render::context& ctx);
	//@}
	/// cgv::gui::provider function to create classic UI
	void create_gui();

	void set_scale_matrix(const mat4& _scale_matrix);
	void set_cell_types(const std::unordered_map<std::string, cell_type>& _cell_types);
	void set_cells(const std::vector<cell>* _cells, size_t _cells_start, size_t _cells_end, const ivec3& extents);
	void unset_cells();

	/// clipping planes
	void create_clipping_plane(const vec3& origin, const vec3& direction);
	void delete_clipping_plane(size_t index, size_t count = 1);
	void clear_clipping_planes();

	void update_clipping_plane(size_t index, const vec3& origin, const vec3& direction);

	// torch
	void set_torch(bool _burn, const vec3& _unscaled_burn_center = vec3(), float _burn_distance = 0);

	// visibility
	void toggle_cell_visibility(size_t cell_index);

	// peel
	void peel(size_t cell_index, size_t node_index);
private:
	void transmit_cells(cgv::render::context& ctx);

	void set_nodes_group_geometry(cgv::render::context& ctx, clipped_box_renderer& br);
	void set_nodes_geometry(cgv::render::context& ctx, clipped_box_renderer& br);

	void set_centers_group_geometry(cgv::render::context& ctx, control_sphere_renderer& br);
	void set_centers_geometry(cgv::render::context& ctx, control_sphere_renderer& br);

	/// color map
	void add_color_points(const rgba& color0, const rgba& color1);
	void update_color_point(size_t index, const rgba& color0, const rgba& color1);

	void interpolate_colors(bool force = false);

	void point_at_cell(size_t cell_index, size_t node_index = SIZE_MAX) const;
};

typedef cgv::data::ref_ptr<cells_container> cells_container_ptr;
