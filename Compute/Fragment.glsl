#version 430 core

in vec2 texcoord;
out vec4 color;

uniform sampler2D tex;
const float GAMMA = 2.2;

void main() {
	color = texture(tex, texcoord);
	color = pow(color, vec4(1.0 / GAMMA));
}