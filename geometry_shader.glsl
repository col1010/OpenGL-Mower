#version 450

layout(triangles, invocations=2) in;
in vec4 fca[]; 
layout(line_strip, max_vertices=6) out;
out vec4 fcolor;
uniform mat4 mvp;

vec4 n4_test(vec4 p){
	vec3 offset = p.xyz;
	return vec4(2 * normalize(offset), 1);
}

void main(void) {
	
	if(gl_InvocationID == 0){

		gl_Position = mvp * gl_in[0].gl_Position;
	 	fcolor = fca[0];
		EmitVertex();
		gl_Position = mvp * gl_in[1].gl_Position;
	 	fcolor = fca[1];
		EmitVertex();
		gl_Position = mvp * gl_in[2].gl_Position;
	 	fcolor = fca[2];
		EmitVertex();
		gl_Position = mvp * gl_in[0].gl_Position;
	 	fcolor = fca[0];
		EmitVertex();
	} else {
		gl_Position = mvp * n4_test(gl_in[0].gl_Position);
 		fcolor = fca[0];
		EmitVertex();
		gl_Position = mvp * n4_test(gl_in[1].gl_Position);
 		fcolor = fca[1];
		EmitVertex();
		gl_Position = mvp * n4_test(gl_in[2].gl_Position);
 		fcolor = fca[2];
		EmitVertex();
		gl_Position = mvp * n4_test(gl_in[0].gl_Position);
 		fcolor = fca[0];
		EmitVertex();
	}
	EndPrimitive();
}

