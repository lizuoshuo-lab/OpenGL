#version 460 core

layout (location = 0) in vec3 aPos;

out vec3 worldRay;

uniform mat4 viewMatrix;
uniform mat4 projectionMatrix;

void main()
{
	mat4 viewRotation = mat4(mat3(viewMatrix));
	vec4 clipPosition = projectionMatrix * viewRotation * vec4(aPos, 1.0);
	gl_Position = clipPosition.xyww;
	worldRay = aPos;
}
