#version 460

in vec3 in_vertex;
in vec3 in_color;
uniform mat4 mvp;
out vec4 fca;
//out vec4 gl_Position;

void main(void) {
	gl_Position = mvp * vec4(in_vertex * 50 - vec3(0,0.01,0), 1.0);
	fca = vec4(in_color, 1.0);
}
