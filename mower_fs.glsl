#version 460

out vec4 outcolor;
in vec2 f_texcoord;
uniform sampler2D tex;

void main(void) {
  outcolor = texture(tex, vec2(f_texcoord.x, f_texcoord.y));
//  outcolor = vec4(1, 1, 1, 1);
}
