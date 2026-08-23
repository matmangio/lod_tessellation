#version 410 core

// UNIFORMS and INPUTS
layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normal;

uniform mat4 model_matrix;
uniform mat3 normal_matrix;
uniform mat4 view_matrix;
uniform mat4 projection_matrix;

uniform vec3 light_direction;

// OUTPUTS
out vec3 N;
out vec3 L;
out vec3 V;

void main() {
	vec4 mv_position = view_matrix * model_matrix * vec4(position, 1.0);

  	L = normalize(light_direction);
	N = normalize(normal_matrix * normal);
	V = -mv_position.xyz;

    gl_Position = projection_matrix * mv_position;
}