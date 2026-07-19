#version 460 core

layout (location = 0) out vec4 gPositionAo;
layout (location = 1) out vec4 gNormalRoughness;
layout (location = 2) out vec4 gAlbedoMetallic;

in vec2 uv;
in vec3 worldPosition;
in mat3 tbn;
flat in vec3 instanceOrigin;
flat in float lodFade;

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
uniform float surfaceVariation;
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

float hash13(vec3 value)
{
	value = fract(value * 0.1031);
	value += dot(value, value.yzx + 33.33);
	return fract((value.x + value.y) * value.z);
}

float lodDither(vec2 pixel)
{
	return fract(52.9829189 * fract(dot(floor(pixel), vec2(0.06711056, 0.00583715))));
}

void applyLodFade()
{
	float threshold = lodDither(gl_FragCoord.xy);
	if (lodFade > 1.0) {
		if (threshold < lodFade - 1.0) discard;
	}
	else if (threshold >= lodFade) {
		discard;
	}
}

vec3 mineralTint(float seed)
{
	vec3 coolStone = vec3(0.82, 0.90, 1.02);
	vec3 ironStone = vec3(1.12, 0.93, 0.78);
	vec3 tint = mix(coolStone, ironStone, smoothstep(0.12, 0.88, seed));
	return mix(vec3(1.0), tint, surfaceVariation);
}

void main()
{
	applyLodFade();
	vec4 albedoSample = texture(albedoTex, uv);
	float alpha = albedoSample.a * opacity;
	if (alphaMask != 0 && alpha < alphaCutoff) {
		discard;
	}

	vec3 tangentNormal = texture(normalTex, uv).rgb * 2.0 - 1.0;
	tangentNormal.xy *= normalStrength;
	vec3 normal = normalize(tbn * normalize(tangentNormal));
	float surfaceSeed = hash13(instanceOrigin * 0.173 + vec3(7.1, 19.7, 37.3));
	vec3 albedo = albedoSample.rgb * baseColorFactor * mineralTint(surfaceSeed);
	float metallic = clamp(
		sampleChannel(texture(metallicTex, uv), metallicChannel) * metallicScale,
		0.0,
		1.0
	);
	float roughnessVariation = mix(1.0, mix(0.82, 1.12, surfaceSeed), surfaceVariation);
	float roughness = clamp(
		sampleChannel(texture(roughnessTex, uv), roughnessChannel) * roughnessScale * roughnessVariation,
		0.04,
		1.0
	);
	float ao = clamp(sampleChannel(texture(aoTex, uv), aoChannel) * aoScale, 0.0, 1.0);

	gPositionAo = vec4(worldPosition, ao);
	gNormalRoughness = vec4(normal, roughness);
	gAlbedoMetallic = vec4(albedo, metallic);
}
