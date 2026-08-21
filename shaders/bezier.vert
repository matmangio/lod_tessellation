#version 410 core

// UNIFORMS and INPUTS
layout (location = 0) in vec3 position;

uniform mat4 model_matrix;
uniform mat4 view_matrix;

uniform vec3 light_position;

// OUTPUTS
out vec3 L_cs;
out vec3 V_cs;

void main() {
	vec4 mv_position = view_matrix * model_matrix * vec4(position, 1.0);
	vec4 v_light_pos = view_matrix * vec4(light_position, 1.0);

	// Compute L and V (N will be evaluated in TES later)
  	L_cs = normalize(v_light_pos.xyz - mv_position.xyz);
	V_cs = -mv_position.xyz;

	// Exclude projection since the TES will need the points in View Space to compute normals
    gl_Position = mv_position;
}