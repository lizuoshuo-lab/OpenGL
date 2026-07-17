#version 460 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aUV;
layout (location = 2) in vec3 aNormal;
layout (location = 3) in vec3 aTangent;
layout (location = 4) in mat4 aInstanceMatrix;
out vec2 uv;
out vec3 normal;
out vec3 worldPosition;
out mat3 tbn;
out vec4 softShadowPosition;
flat out vec3 instanceOrigin;
uniform mat4 modelMatrix;
uniform mat4 viewMatrix;
uniform mat4 projectionMatrix;
uniform mat4 softShadowMatrix;
uniform int instanced;

uniform mat3 normalMatrix;

void main()
{
	mat4 renderModel = modelMatrix;
	if (instanced != 0) {
		renderModel = modelMatrix * aInstanceMatrix;
	}
	vec4 transformPosition = renderModel * vec4(aPos, 1.0);
	worldPosition = transformPosition.xyz;
	instanceOrigin = renderModel[3].xyz;
	softShadowPosition = softShadowMatrix * transformPosition;

	gl_Position = projectionMatrix * viewMatrix * transformPosition;
	
	uv = aUV;
	mat3 renderNormalMatrix = instanced != 0
		? transpose(inverse(mat3(renderModel)))
		: normalMatrix;
	normal = normalize(renderNormalMatrix * aNormal);
	vec3 tangent = normalize(renderNormalMatrix * aTangent);
	tangent = normalize(tangent - dot(tangent, normal) * normal);
	vec3 bitangent = normalize(cross(normal, tangent));
	tbn = mat3(tangent, bitangent, normal);
}
