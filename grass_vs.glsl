#version 460

in vec3 in_vertex;
uniform vec3 top_color;
uniform vec3 bottom_color;
uniform mat4 mvp;
out vec4 fca;
uniform int count;
uniform vec4 mower_pos;
uniform int finished_mowing;
uniform int mower_scale;
uniform float grass_len_modifier;
uniform float wind_speed;
//out vec4 gl_Position;

layout(std430, binding = 0) buffer grasslocs {
	float grasslocbuf[];
};

layout(std430, binding = 1) buffer grasslen {
	float grasslenbuf[];
};

void main(void) {
	//gl_Position = mvp * vec4(in_vertex, 1.0);

	vec4 offset = vec4(grasslocbuf[gl_InstanceID*2], 0, grasslocbuf[gl_InstanceID*2+1], 0);
	vec4 pos = vec4(in_vertex, 1.0) + offset;

	if (pos.y > 0) { // if the vertex is above 0 on the y axis (this selects only the vertices at the top of the blades of grass)
		// 40 / mower_scale because the original "hitbox" of the full-size mower would be a +-40 square. at a scaling of n, 40 / n gives the correct "hitbox"

		if (((pos.x < mower_pos.x + 40 / float(mower_scale)) && (pos.x > mower_pos.x - 40 / float(mower_scale)) && ((pos.z < mower_pos.z + 40 / float(mower_scale)) && (pos.z > mower_pos.z - 40 / float(mower_scale))))) {
			
			if (finished_mowing == 1) {
				pos.y = grasslenbuf[gl_InstanceID * 2 + 1] + grass_len_modifier;
				grasslenbuf[gl_InstanceID * 2] = grasslenbuf[gl_InstanceID * 2 + 1]; // reset the cut grass
			} else {
				pos.y = grasslenbuf[gl_InstanceID * 2] = 0.05;
			}

		} else {
			if (grasslenbuf[gl_InstanceID * 2] > 0.05)
				pos.y = grasslenbuf[gl_InstanceID * 2] + grass_len_modifier; // modify the top vertex's y coordinate to change the length of each blade of grass
			else
				pos.y = 0.05;
		}

		if (grasslenbuf[gl_InstanceID * 2] > 0.05)
			pos.x += (sin(count * wind_speed + grasslenbuf[gl_InstanceID * 2] * 10)) / (5.0 * (grass_len_modifier + 1)) * grasslenbuf[gl_InstanceID * 2] * ((grass_len_modifier + 1) * (grass_len_modifier + 1)); // simulate wind blowing the grass side to side, and scale its effect using each blade's length

		fca = vec4(top_color, 1.0);
	} else {
		fca = vec4(bottom_color, 1.0);
	}
	gl_Position = mvp * pos;
}
