#version 460 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aUV;
layout (location = 2) in vec3 aNormal;
layout (location = 3) in vec3 aTangent;
layout (location = 4) in mat4 aInstanceMatrix;

out vec2 uv;
out vec3 worldPosition;
out mat3 tbn;
flat out vec3 instanceOrigin;

uniform mat4 modelMatrix;
uniform mat4 viewMatrix;
uniform mat4 projectionMatrix;
uniform int instanced;

void main()
{
	mat4 renderModel = modelMatrix;
	if (instanced != 0) {
		renderModel = modelMatrix * aInstanceMatrix;
	}

	vec4 position = renderModel * vec4(aPos, 1.0);
	mat3 normalMatrix = transpose(inverse(mat3(renderModel)));
	vec3 normal = normalize(normalMatrix * aNormal);
	vec3 tangent = normalize(normalMatrix * aTangent);
	tangent = normalize(tangent - dot(tangent, normal) * normal);
	vec3 bitangent = normalize(cross(normal, tangent));

	uv = aUV;
	worldPosition = position.xyz;
	instanceOrigin = renderModel[3].xyz;
	tbn = mat3(tangent, bitangent, normal);
	gl_Position = projectionMatrix * viewMatrix * position;
}
