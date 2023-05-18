#version 460

in vec3 in_vertex;
in vec3 in_color;
uniform mat4 mvp;
out vec4 fca;
uniform vec2 grasslocs[5000];
//out vec4 gl_Position;

void main(void) {
	//gl_Position = mvp * vec4(in_vertex, 1.0);
 	vec4 offset = vec4(grasslocs[gl_InstanceID].x, 0, grasslocs[gl_InstanceID].y, 0);
	gl_Position = (vec4(in_vertex, 1.0) + offset) * mvp;
	fca = vec4(in_color, 1.0);
}
