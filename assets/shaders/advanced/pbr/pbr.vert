#version 460 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aUV;
layout (location = 2) in vec3 aNormal;
layout (location = 3) in vec3 aTangent;
out vec2 uv;
out vec3 normal;
out vec3 worldPosition;
out mat3 tbn;
uniform mat4 modelMatrix;
uniform mat4 viewMatrix;
uniform mat4 projectionMatrix;

uniform mat3 normalMatrix;

void main()
{
	vec4 transformPosition = vec4(aPos, 1.0);
	transformPosition = modelMatrix * transformPosition;
	worldPosition = transformPosition.xyz;

	gl_Position = projectionMatrix * viewMatrix * transformPosition;
	
	uv = aUV;
	normal = normalize(normalMatrix * aNormal);
	vec3 tangent = normalize(normalMatrix * aTangent);
	tangent = normalize(tangent - dot(tangent, normal) * normal);
	vec3 bitangent = normalize(cross(normal, tangent));
	tbn = mat3(tangent, bitangent, normal);
}
