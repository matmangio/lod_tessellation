#version 410 core

// UNIFORMS and INPUTS
layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normal;

uniform mat4 model_matrix;
uniform mat3 normal_matrix;
uniform mat4 view_matrix;

uniform vec3 camera_position;
uniform vec3 light_direction;

// OUTPUTS
out vec3 N_cs;
out vec3 L_cs;
out vec3 V_cs;
out float camera_distance_cs;

void main() {
	vec4 mv_position = view_matrix * model_matrix * vec4(position, 1.0);

	// Compute L, N and V
  	L_cs = normalize(light_direction);
	N_cs = normalize(normal_matrix * normal);
	V_cs = -mv_position.xyz;

    // Exclude projection since the TES will need the points in View Space to compute normals
    gl_Position = mv_position;
}