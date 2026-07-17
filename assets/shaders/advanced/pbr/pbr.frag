#version 460 core
out vec4 FragColor;

in vec2 uv;
in vec3 normal;
in vec3 worldPosition;
in mat3 tbn;
in vec4 softShadowPosition;

uniform vec3 cameraPosition;

#include "../../common/lightStruct.glsl"

uniform PointLight pointLights[4];
uniform sampler2D albedoTex;
uniform sampler2D normalTex;
uniform sampler2D metallicTex;
uniform sampler2D roughnessTex;
uniform sampler2D aoTex;
uniform samplerCube irradianceMap;
uniform samplerCube prefilterMap;
uniform sampler2D brdfLUT;
uniform float envIntensity;
uniform float maxReflectionLod;
uniform float metallicScale;
uniform float roughnessScale;
uniform float aoScale;
uniform float normalStrength;
uniform vec3 baseColorFactor;
uniform float opacity;
uniform int alphaMask;
uniform float alphaCutoff;
uniform int metallicChannel;
uniform int roughnessChannel;
uniform int aoChannel;
uniform int debugView;
uniform int softShadowEnabled;
uniform sampler2D softShadowMap;
uniform vec3 softShadowLightPosition;
uniform float softShadowStrength;
uniform float softShadowBias;
uniform float softShadowRadius;

const float PI = 3.141592653589793;

float sampleChannel(vec4 value, int channel)
{
	if (channel == 1) return value.g;
	if (channel == 2) return value.b;
	if (channel == 3) return value.a;
	return value.r;
}

float calculateSoftShadow(vec3 surfaceNormal)
{
	if (softShadowEnabled == 0 || softShadowPosition.w <= 0.0) {
		return 0.0;
	}

	vec3 projected = softShadowPosition.xyz / softShadowPosition.w;
	projected = projected * 0.5 + 0.5;
	if (projected.z <= 0.0 || projected.z >= 1.0 ||
		projected.x <= 0.0 || projected.x >= 1.0 ||
		projected.y <= 0.0 || projected.y >= 1.0) {
		return 0.0;
	}

	const vec2 poissonDisk[16] = vec2[](
		vec2(-0.94201624, -0.39906216),
		vec2( 0.94558609, -0.76890725),
		vec2(-0.09418410, -0.92938870),
		vec2( 0.34495938,  0.29387760),
		vec2(-0.91588581,  0.45771432),
		vec2(-0.81544232, -0.87912464),
		vec2(-0.38277543,  0.27676845),
		vec2( 0.97484398,  0.75648379),
		vec2( 0.44323325, -0.97511554),
		vec2( 0.53742981, -0.47373420),
		vec2(-0.26496911, -0.41893023),
		vec2( 0.79197514,  0.19090188),
		vec2(-0.24188840,  0.99706507),
		vec2(-0.81409955,  0.91437590),
		vec2( 0.19984126,  0.78641367),
		vec2( 0.14383161, -0.14100790)
	);

	vec3 lightDirection = normalize(softShadowLightPosition - worldPosition);
	float normalBias = max(softShadowBias * (1.0 - dot(surfaceNormal, lightDirection)), softShadowBias * 0.25);
	vec2 texelSize = 1.0 / vec2(textureSize(softShadowMap, 0));
	float occlusion = 0.0;
	for (int i = 0; i < 16; ++i) {
		float closestDepth = texture(
			softShadowMap,
			projected.xy + poissonDisk[i] * texelSize * softShadowRadius
		).r;
		occlusion += projected.z - normalBias > closestDepth ? 1.0 : 0.0;
	}

	return (occlusion / 16.0) * softShadowStrength;
}

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
	float a = roughness * roughness;
	float a2 = a * a;
	float NdotH = max(dot(N, H), 0.0);
	float NdotH2 = NdotH * NdotH;

	float denom = NdotH2 * (a2 - 1.0) + 1.0;
	denom = PI * denom * denom;
	return a2 / max(denom, 0.000001);
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
	float r = roughness + 1.0;
	float k = (r * r) / 8.0;
	float denom = NdotV * (1.0 - k) + k;
	return NdotV / max(denom, 0.000001);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
	float NdotV = max(dot(N, V), 0.0);
	float NdotL = max(dot(N, L), 0.0);
	float ggx2 = GeometrySchlickGGX(NdotV, roughness);
	float ggx1 = GeometrySchlickGGX(NdotL, roughness);
	return ggx1 * ggx2;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
	float f = pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
	return F0 + (1.0 - F0) * f;
}

vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
	float f = pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
	return F0 + (max(vec3(1.0 - roughness), F0) - F0) * f;
}

void main()
{
	vec4 albedoSample = texture(albedoTex, uv);
	float alpha = albedoSample.a * opacity;
	if (alphaMask != 0 && alpha < alphaCutoff) {
		discard;
	}
	vec3 albedo = albedoSample.rgb * baseColorFactor;
	float metallic = clamp(sampleChannel(texture(metallicTex, uv), metallicChannel) * metallicScale, 0.0, 1.0);
	float roughness = clamp(sampleChannel(texture(roughnessTex, uv), roughnessChannel) * roughnessScale, 0.04, 1.0);
	float ao = clamp(sampleChannel(texture(aoTex, uv), aoChannel) * aoScale, 0.0, 1.0);

	vec3 tangentNormal = texture(normalTex, uv).rgb * 2.0 - 1.0;
	tangentNormal.xy *= normalStrength;
	tangentNormal = normalize(tangentNormal);
	vec3 N = normalize(tbn * tangentNormal);
	vec3 V = normalize(cameraPosition - worldPosition);
	vec3 R = reflect(-V, N);
	float NdotV = max(dot(N, V), 0.0);
	float softShadow = calculateSoftShadow(N);
	vec3 F0 = vec3(0.04);
	F0 = mix(F0, albedo, metallic);

	vec3 Lo = vec3(0.0);
	for (int i = 0; i < 4; i++) {
		vec3 L = normalize(pointLights[i].position - worldPosition);
		vec3 H = normalize(V + L);
		float distance = length(pointLights[i].position - worldPosition);
		float attenuation = 1.0 / max(distance * distance, 0.000001);
		vec3 radiance = pointLights[i].color * attenuation;

		float NdotL = max(dot(N, L), 0.0);
		float D = DistributionGGX(N, H, roughness);
		float G = GeometrySmith(N, V, L, roughness);
		vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);

		vec3 numerator = D * G * F;
		float denominator = max(4.0 * NdotV * NdotL, 0.000001);
		vec3 specular = numerator / denominator;

		vec3 kS = F;
		vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);

		float directVisibility = i == 0 ? 1.0 - softShadow : 1.0;
		Lo += (kD * albedo / PI + specular) * radiance * NdotL * directVisibility;
	}

	vec3 F = fresnelSchlickRoughness(NdotV, F0, roughness);
	vec3 kS = F;
	vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);

	vec3 irradiance = texture(irradianceMap, N).rgb;
	vec3 diffuse = irradiance * albedo;

	vec3 prefilteredColor = textureLod(prefilterMap, R, roughness * maxReflectionLod).rgb;
	vec2 envBRDF = texture(brdfLUT, vec2(NdotV, roughness)).rg;
	vec3 specular = prefilteredColor * (F * envBRDF.x + envBRDF.y);

	vec3 diffuseIbl = kD * diffuse;
	vec3 specularIbl = specular;
	vec3 ambient = (diffuseIbl + specularIbl) * ao * envIntensity;
	vec3 color = ambient + Lo;

	if (debugView == 1) {
		color = Lo;
	}
	else if (debugView == 2) {
		color = ambient;
	}
	else if (debugView == 3) {
		color = diffuseIbl * ao * envIntensity;
	}
	else if (debugView == 4) {
		color = specularIbl * ao * envIntensity;
	}
	else if (debugView == 5) {
		color = irradiance;
	}
	else if (debugView == 6) {
		color = prefilteredColor;
	}
	else if (debugView == 7) {
		color = vec3(envBRDF, 0.0);
	}
	else if (debugView == 8) {
		color = N * 0.5 + 0.5;
	}
	else if (debugView == 9) {
		color = vec3(ao);
	}
	else if (debugView == 10) {
		color = vec3(roughness);
	}
	else if (debugView == 11) {
		color = vec3(metallic);
	}

	FragColor = vec4(color, alpha);
}
