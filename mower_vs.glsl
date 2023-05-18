#version 460

in vec3 in_vertex;
in vec2 in_texcoord;
uniform mat4 in_model;
uniform mat4 view;
uniform mat4 projection;
out vec2 f_texcoord;
uniform vec3 pos;
uniform int mower_scale;

void main(void) {
	vec3 position = in_vertex + pos;
	
	gl_Position = projection * view * in_model * vec4(position / float(mower_scale), 1.0);
	f_texcoord = in_texcoord;
}
