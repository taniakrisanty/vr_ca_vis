// This source code is adapted from simple_primitive_container in vr_lab_test plugin

#pragma once

#include <cgv/base/node.h>
#include <cgv/render/drawable.h>
#include <cg_nui/focusable.h>
#include <cg_nui/pointable.h>
#include <cg_nui/grabable.h>
#include <cgv/gui/provider.h>
#include <cgv_gl/sphere_renderer.h>

#include <unordered_set>

#include "cell_data.h"
#include "clipped_box_renderer.h"
#include "regular_grid.h"

class cells_container_listener
{
public:
	virtual void on_cell_grabbed(size_t offset, size_t index) = 0;
};

class cells_container :
	public cgv::base::node,
	public cgv::render::drawable,
	public cgv::nui::focusable,
	public cgv::nui::grabable,
	public cgv::nui::pointable,
	public cgv::gui::provider
{
	cgv::render::box_render_style brs;
	cgv::render::sphere_render_style srs;
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

	bool show_gui;

	// acceleration data structure
	regular_grid<cell> grid;

	// geometry of cubes with color
	vec3 extent;
	quat rotation;

	mat4 scale_matrix;
	// inv_scale_matrix is used to prevent expensive scaling of all cells
	mat4 inv_scale_matrix;

	// cell types (ct1, ct2, etc) and their visibility
	std::unordered_set<std::string> cell_types;
	std::vector<int> cell_type_visibilities;

	// offset set by time_step_start
	size_t offset;
	std::vector<cell> cells;

	// clipping planes that are used by clipped_box geometry shader
	// in the form of ax + by + cz + d = 0
	std::vector<vec4> clipping_planes;
	const std::vector<vec3>* clipping_plane_origins = NULL;
	const std::vector<vec3>* clipping_plane_directions = NULL;

	// hid with focus on object
	cgv::nui::hid_identifier hid_id;
	// index of focused primitive
	int prim_idx = -1;
	// state of object
	state_enum state = state_enum::idle;
	/// return color modified based on state
	rgb get_modified_color(const rgb& color) const;
public:
	cells_container(cells_container_listener* _listener, const std::string& _name, const vec3& _extent = vec3(1.f), const quat& _rotation = quat(1, 0, 0, 0));
	/// return type name
	std::string get_type_name() const;
	/// reflect member variables
	bool self_reflect(cgv::reflect::reflection_handler& rh);
	/// callback on member updates to keep data structure consistent
	void on_set(void* member_ptr);
	//@name cgv::nui::focusable interface
	//@{
	bool focus_change(cgv::nui::focus_change_action action, cgv::nui::refocus_action rfa, const cgv::nui::focus_demand& demand, const cgv::gui::event& e, const cgv::nui::dispatch_info& dis_info);
	void stream_help(std::ostream& os);
	bool handle(const cgv::gui::event& e, const cgv::nui::dispatch_info& dis_info, cgv::nui::focus_request& request);
	//@}

	/// implement closest point algorithm and return whether this was found (failure only for invisible objects) and in this case set \c prj_point to closest point and \c prj_normal to corresponding surface normal
	bool compute_closest_point(const vec3& point, vec3& prj_point, vec3& prj_normal, size_t& primitive_idx);
	/// implement ray object intersection and return whether intersection was found and in this case set \c hit_param to ray parameter and optionally \c hit_normal to surface normal of intersection
	bool compute_intersection(const vec3& ray_start, const vec3& ray_direction, float& hit_param, vec3& hit_normal, size_t& primitive_idx);

	//@name cgv::render::drawable interface
	//@{
	/// initialization called once per context creation
	bool init(cgv::render::context& ctx);
	/// called before context destruction to clean up GPU objects
	void clear(cgv::render::context& ctx);
	/// draw scene here
	void draw(cgv::render::context& ctx);
	//@}
	/// cgv::gui::provider function to create classic UI
	void create_gui();

	void set_scale_matrix(const mat4& _scale_matrix);
	void set_cell_types(const std::unordered_set<std::string>& _cell_types);
	void set_cells(size_t _offset, std::vector<cell>::const_iterator cells_begin, std::vector<cell>::const_iterator cells_end);
	void set_clipping_planes(const std::vector<vec4>& _clipping_planes);
	void set_clipping_planes(const std::vector<vec3>* _clipping_plane_origins, const std::vector<vec3>* _clipping_plane_directions);
private:
	void grab_cell(size_t index) const;
};

typedef cgv::data::ref_ptr<cells_container> cells_container_ptr;
