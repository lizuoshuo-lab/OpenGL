#version 460 core

layout (location = 0) out vec4 FragColor;

in vec2 uv;

#include "../common/lightStruct.glsl"

uniform sampler2D gPositionAo;
uniform sampler2D gNormalRoughness;
uniform sampler2D gAlbedoMetallic;
uniform sampler2D ssaoTexture;
uniform samplerCube irradianceMap;
uniform samplerCube prefilterMap;
uniform sampler2D brdfLUT;
uniform sampler2D softShadowMap;
uniform PointLight pointLights[4];
uniform vec3 cameraPosition;
uniform vec3 clearColor;
uniform vec3 softShadowLightPosition;
uniform mat4 softShadowMatrix;
uniform float envIntensity;
uniform float maxReflectionLod;
uniform float ssaoStrength;
uniform float softShadowStrength;
uniform float softShadowBias;
uniform float softShadowRadius;
uniform int ssaoEnabled;
uniform int softShadowEnabled;
uniform int lightingDebugView;

const float PI = 3.141592653589793;

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
	float a = roughness * roughness;
	float a2 = a * a;
	float NdotH = max(dot(N, H), 0.0);
	float denominator = NdotH * NdotH * (a2 - 1.0) + 1.0;
	return a2 / max(PI * denominator * denominator, 0.000001);
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
	float r = roughness + 1.0;
	float k = (r * r) / 8.0;
	return NdotV / max(NdotV * (1.0 - k) + k, 0.000001);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
	return GeometrySchlickGGX(max(dot(N, V), 0.0), roughness) *
		GeometrySchlickGGX(max(dot(N, L), 0.0), roughness);
}

vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
	return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
	return F0 + (max(vec3(1.0 - roughness), F0) - F0) *
		pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float calculateSoftShadow(vec3 worldPosition, vec3 surfaceNormal)
{
	if (softShadowEnabled == 0) {
		return 0.0;
	}
	vec4 shadowPosition = softShadowMatrix * vec4(worldPosition, 1.0);
	if (shadowPosition.w <= 0.0) {
		return 0.0;
	}

	vec3 projected = shadowPosition.xyz / shadowPosition.w;
	projected = projected * 0.5 + 0.5;
	if (projected.z <= 0.0 || projected.z >= 1.0 ||
		projected.x <= 0.0 || projected.x >= 1.0 ||
		projected.y <= 0.0 || projected.y >= 1.0) {
		return 0.0;
	}

	const vec2 poissonDisk[16] = vec2[](
		vec2(-0.94201624, -0.39906216), vec2( 0.94558609, -0.76890725),
		vec2(-0.09418410, -0.92938870), vec2( 0.34495938,  0.29387760),
		vec2(-0.91588581,  0.45771432), vec2(-0.81544232, -0.87912464),
		vec2(-0.38277543,  0.27676845), vec2( 0.97484398,  0.75648379),
		vec2( 0.44323325, -0.97511554), vec2( 0.53742981, -0.47373420),
		vec2(-0.26496911, -0.41893023), vec2( 0.79197514,  0.19090188),
		vec2(-0.24188840,  0.99706507), vec2(-0.81409955,  0.91437590),
		vec2( 0.19984126,  0.78641367), vec2( 0.14383161, -0.14100790)
	);

	vec3 lightDirection = normalize(softShadowLightPosition - worldPosition);
	float normalBias = max(
		softShadowBias * (1.0 - dot(surfaceNormal, lightDirection)),
		softShadowBias * 0.25
	);
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

void main()
{
	vec4 normalRoughness = texture(gNormalRoughness, uv);
	if (length(normalRoughness.xyz) < 0.1) {
		FragColor = vec4(clearColor, 1.0);
		return;
	}

	vec4 positionAo = texture(gPositionAo, uv);
	vec4 albedoMetallic = texture(gAlbedoMetallic, uv);
	vec3 worldPosition = positionAo.xyz;
	vec3 N = normalize(normalRoughness.xyz);
	vec3 albedo = albedoMetallic.rgb;
	float metallic = albedoMetallic.a;
	float roughness = normalRoughness.a;
	float materialAo = positionAo.a;
	float screenAo = ssaoEnabled != 0 ? texture(ssaoTexture, uv).r : 1.0;
	float combinedAo = materialAo * mix(1.0, screenAo, ssaoStrength);

	vec3 V = normalize(cameraPosition - worldPosition);
	vec3 R = reflect(-V, N);
	float NdotV = max(dot(N, V), 0.0);
	vec3 F0 = mix(vec3(0.04), albedo, metallic);
	float softShadow = calculateSoftShadow(worldPosition, N);

	vec3 directLight = vec3(0.0);
	for (int i = 0; i < 4; ++i) {
		vec3 toLight = pointLights[i].position - worldPosition;
		float distance = length(toLight);
		vec3 L = toLight / max(distance, 0.000001);
		vec3 H = normalize(V + L);
		vec3 radiance = pointLights[i].color / max(distance * distance, 0.000001);
		float NdotL = max(dot(N, L), 0.0);
		float D = DistributionGGX(N, H, roughness);
		float G = GeometrySmith(N, V, L, roughness);
		vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);
		vec3 specular = D * G * F / max(4.0 * NdotV * NdotL, 0.000001);
		vec3 diffuseWeight = (vec3(1.0) - F) * (1.0 - metallic);
		float visibility = i == 0 ? 1.0 - softShadow : 1.0;
		directLight += (diffuseWeight * albedo / PI + specular) * radiance * NdotL * visibility;
	}

	vec3 F = fresnelSchlickRoughness(NdotV, F0, roughness);
	vec3 diffuseWeight = (vec3(1.0) - F) * (1.0 - metallic);
	vec3 irradiance = texture(irradianceMap, N).rgb;
	vec3 diffuseIbl = diffuseWeight * irradiance * albedo;
	vec3 prefiltered = textureLod(prefilterMap, R, roughness * maxReflectionLod).rgb;
	vec2 envBrdf = texture(brdfLUT, vec2(NdotV, roughness)).rg;
	vec3 specularIbl = prefiltered * (F * envBrdf.x + envBrdf.y);
	vec3 ambient = (diffuseIbl + specularIbl) * combinedAo * envIntensity;
	vec3 color = directLight + ambient;

	if (lightingDebugView == 1) color = directLight;
	else if (lightingDebugView == 2) color = ambient;
	else if (lightingDebugView == 3) color = diffuseIbl * combinedAo * envIntensity;
	else if (lightingDebugView == 4) color = specularIbl * combinedAo * envIntensity;
	else if (lightingDebugView == 5) color = irradiance;
	else if (lightingDebugView == 6) color = prefiltered;
	else if (lightingDebugView == 7) color = vec3(envBrdf, 0.0);
	else if (lightingDebugView == 8) color = N * 0.5 + 0.5;
	else if (lightingDebugView == 9) color = vec3(combinedAo);
	else if (lightingDebugView == 10) color = vec3(roughness);
	else if (lightingDebugView == 11) color = vec3(metallic);

	FragColor = vec4(color, 1.0);
}
