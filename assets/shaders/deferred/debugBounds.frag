#version 460 core

layout (location = 0) out vec4 FragColor;

uniform vec3 boundsColor;

void main()
{
	FragColor = vec4(boundsColor, 1.0);
}
