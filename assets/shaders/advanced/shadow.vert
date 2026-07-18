#version 460 core
layout(location = 0) in vec3 aPos;
layout(location = 4) in mat4 aInstanceMatrix;
layout(location = 8) in ivec4 aBoneIds;
layout(location = 9) in vec4 aBoneWeights;

uniform mat4 lightMatrix;// lightProjection * lightView
uniform mat4 modelMatrix;
uniform int instanced;
uniform int skinned;
uniform mat4 boneMatrices[128];

void main() {
	mat4 renderModel = instanced != 0
		? modelMatrix * aInstanceMatrix
		: modelMatrix;
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
	gl_Position = lightMatrix * renderModel * skinMatrix * vec4(aPos, 1.0);
}
