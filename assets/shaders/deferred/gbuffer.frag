#version 460 core

layout (location = 0) out vec4 gPositionAo;
layout (location = 1) out vec4 gNormalRoughness;
layout (location = 2) out vec4 gAlbedoMetallic;

in vec2 uv;
in vec3 worldPosition;
in mat3 tbn;

uniform sampler2D albedoTex;
uniform sampler2D normalTex;
uniform sampler2D metallicTex;
uniform sampler2D roughnessTex;
uniform sampler2D aoTex;
uniform vec3 baseColorFactor;
uniform float metallicScale;
uniform float roughnessScale;
uniform float aoScale;
uniform float normalStrength;
uniform float opacity;
uniform int metallicChannel;
uniform int roughnessChannel;
uniform int aoChannel;
uniform int alphaMask;
uniform float alphaCutoff;

float sampleChannel(vec4 value, int channel)
{
	if (channel == 1) return value.g;
	if (channel == 2) return value.b;
	if (channel == 3) return value.a;
	return value.r;
}

void main()
{
	vec4 albedoSample = texture(albedoTex, uv);
	float alpha = albedoSample.a * opacity;
	if (alphaMask != 0 && alpha < alphaCutoff) {
		discard;
	}

	vec3 tangentNormal = texture(normalTex, uv).rgb * 2.0 - 1.0;
	tangentNormal.xy *= normalStrength;
	vec3 normal = normalize(tbn * normalize(tangentNormal));
	vec3 albedo = albedoSample.rgb * baseColorFactor;
	float metallic = clamp(
		sampleChannel(texture(metallicTex, uv), metallicChannel) * metallicScale,
		0.0,
		1.0
	);
	float roughness = clamp(
		sampleChannel(texture(roughnessTex, uv), roughnessChannel) * roughnessScale,
		0.04,
		1.0
	);
	float ao = clamp(sampleChannel(texture(aoTex, uv), aoChannel) * aoScale, 0.0, 1.0);

	gPositionAo = vec4(worldPosition, ao);
	gNormalRoughness = vec4(normal, roughness);
	gAlbedoMetallic = vec4(albedo, metallic);
}
