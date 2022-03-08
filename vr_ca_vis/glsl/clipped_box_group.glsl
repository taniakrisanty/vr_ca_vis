#version 150

/*
The following interface is implemented in this shader:
//***** begin interface of group.glsl ***********************************
float group_visibility(in float visibility, int group_index);
//***** end interface of group.glsl ***********************************
*/

//layout(std430, binding = 0) buffer visibility_ssbo
//{
//    float visibilities[];
//};

uniform bool use_group_visibility = false;
uniform float group_visibilities[250];

float group_visibility(in float visible, int group_index)
{
	if (use_group_visibility) {
		return group_visibilities[group_index];
	}
	else {
		return visible;
	}
}