#version 460 core

layout (location = 0) out float FragOcclusion;

in vec2 uv;

uniform sampler2D gPositionAo;
uniform sampler2D gNormalRoughness;
uniform sampler2D noiseTexture;
uniform vec3 samples[32];
uniform int sampleCount;
uniform float radius;
uniform float bias;
uniform mat4 viewMatrix;
uniform mat4 projectionMatrix;

void main()
{
	vec3 worldPosition = texture(gPositionAo, uv).xyz;
	vec3 normal = texture(gNormalRoughness, uv).xyz;
	if (length(normal) < 0.1) {
		FragOcclusion = 1.0;
		return;
	}
	normal = normalize(normal);

	vec2 noiseScale = vec2(textureSize(gPositionAo, 0)) / 4.0;
	vec3 randomVector = normalize(texture(noiseTexture, uv * noiseScale).xyz);
	vec3 tangent = normalize(randomVector - normal * dot(randomVector, normal));
	vec3 bitangent = cross(normal, tangent);
	mat3 tbn = mat3(tangent, bitangent, normal);

	float centerViewDepth = (viewMatrix * vec4(worldPosition, 1.0)).z;
	float occlusion = 0.0;
	for (int i = 0; i < sampleCount; ++i) {
		vec3 sampleWorld = worldPosition + tbn * samples[i] * radius;
		vec4 projected = projectionMatrix * viewMatrix * vec4(sampleWorld, 1.0);
		projected.xyz /= max(projected.w, 0.00001);
		vec2 sampleUv = projected.xy * 0.5 + 0.5;
		if (sampleUv.x <= 0.0 || sampleUv.x >= 1.0 || sampleUv.y <= 0.0 || sampleUv.y >= 1.0) {
			continue;
		}

		vec3 sampledWorld = texture(gPositionAo, sampleUv).xyz;
		vec3 sampledNormal = texture(gNormalRoughness, sampleUv).xyz;
		if (length(sampledNormal) < 0.1) {
			continue;
		}
		float sampledViewDepth = (viewMatrix * vec4(sampledWorld, 1.0)).z;
		float expectedViewDepth = (viewMatrix * vec4(sampleWorld, 1.0)).z;
		float rangeWeight = smoothstep(
			0.0,
			1.0,
			radius / max(abs(centerViewDepth - sampledViewDepth), 0.0001)
		);
		occlusion += sampledViewDepth >= expectedViewDepth + bias ? rangeWeight : 0.0;
	}

	FragOcclusion = 1.0 - occlusion / float(max(sampleCount, 1));
}
