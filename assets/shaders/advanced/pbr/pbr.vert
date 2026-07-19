#version 460 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aUV;
layout (location = 2) in vec3 aNormal;
layout (location = 3) in vec3 aTangent;
layout (location = 4) in mat4 aInstanceMatrix;
layout (location = 8) in ivec4 aBoneIds;
layout (location = 9) in vec4 aBoneWeights;
layout (location = 10) in float aLodFade;
out vec2 uv;
out vec3 normal;
out vec3 worldPosition;
out mat3 tbn;
out vec4 softShadowPosition;
flat out vec3 instanceOrigin;
flat out float lodFade;
uniform mat4 modelMatrix;
uniform mat4 viewMatrix;
uniform mat4 projectionMatrix;
uniform mat4 softShadowMatrix;
uniform int instanced;
uniform int skinned;
uniform mat4 boneMatrices[128];

uniform mat3 normalMatrix;

void main()
{
	mat4 renderModel = modelMatrix;
	if (instanced != 0) {
		renderModel = modelMatrix * aInstanceMatrix;
	}

	mat4 skinMatrix = mat4(1.0);
	if (skinned != 0) {
		float totalWeight = dot(aBoneWeights, vec4(1.0));
		if (totalWeight > 0.0001) {
			skinMatrix = (
				boneMatrices[aBoneIds.x] * aBoneWeights.x +
				boneMatrices[aBoneIds.y] * aBoneWeights.y +
				boneMatrices[aBoneIds.z] * aBoneWeights.z +
				boneMatrices[aBoneIds.w] * aBoneWeights.w
			) / totalWeight;
		}
	}

	mat4 vertexModel = renderModel * skinMatrix;
	vec4 transformPosition = vertexModel * vec4(aPos, 1.0);
	worldPosition = transformPosition.xyz;
	instanceOrigin = renderModel[3].xyz;
	lodFade = instanced != 0 ? aLodFade : 1.0;
	softShadowPosition = softShadowMatrix * transformPosition;

	gl_Position = projectionMatrix * viewMatrix * transformPosition;
	
	uv = aUV;
	mat3 renderNormalMatrix = instanced != 0 || skinned != 0
		? transpose(inverse(mat3(vertexModel)))
		: normalMatrix;
	normal = normalize(renderNormalMatrix * aNormal);
	vec3 tangent = normalize(renderNormalMatrix * aTangent);
	tangent = normalize(tangent - dot(tangent, normal) * normal);
	vec3 bitangent = normalize(cross(normal, tangent));
	tbn = mat3(tangent, bitangent, normal);
}
