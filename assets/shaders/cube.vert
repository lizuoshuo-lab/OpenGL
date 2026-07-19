#version 460 core
layout (location = 0) in vec3 aPos;

out vec3 uvw;

uniform mat4 modelMatrix;
uniform mat4 viewMatrix;
uniform mat4 projectionMatrix;
uniform bool fixedBackground;


void main()
{
	mat4 modelRotation = mat4(mat3(modelMatrix));
	mat4 viewRotation = fixedBackground
		? mat4(1.0)
		: mat4(mat3(viewMatrix));
	vec4 clipPosition = projectionMatrix * viewRotation * modelRotation * vec4(aPos, 1.0);
	gl_Position = clipPosition.xyww;
	uvw = mat3(modelMatrix) * aPos;
}
