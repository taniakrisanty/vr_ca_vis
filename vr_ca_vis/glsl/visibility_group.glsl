#version 150

/*
The following interface is implemented in this shader:
//***** begin interface of visibility_group.glsl ***********************************
int visibility(in int visible, int index);
//***** end interface of visibility_group.glsl ***********************************
*/

//layout(std430, binding = 0) buffer visibility_ssbo
//{
//    float visibilities[];
//};

uniform bool use_visibility;
uniform int visibilities[250];

int visibility(in int visible, int index)
{
	if (use_visibility) {
		return visibilities[index];
	}
	else {
		return visible;
	}
}
