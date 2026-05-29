#version 460 core
out vec4 FragColor;

in vec2 uv;
in vec3 normal;
in vec3 worldPosition;
in mat3 tbn;

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

const float PI = 3.141592653589793;

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
	vec3 albedo = texture(albedoTex, uv).rgb;
	float metallic = texture(metallicTex, uv).r;
	float roughness = clamp(texture(roughnessTex, uv).r, 0.04, 1.0);
	float ao = texture(aoTex, uv).r;

	vec3 tangentNormal = texture(normalTex, uv).rgb * 2.0 - 1.0;
	vec3 N = normalize(tbn * tangentNormal);
	vec3 V = normalize(cameraPosition - worldPosition);
	vec3 R = reflect(-V, N);
	float NdotV = max(dot(N, V), 0.0);

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

		Lo += (kD * albedo / PI + specular) * radiance * NdotL;
	}

	vec3 F = fresnelSchlickRoughness(NdotV, F0, roughness);
	vec3 kS = F;
	vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);

	vec3 irradiance = texture(irradianceMap, N).rgb;
	vec3 diffuse = irradiance * albedo;

	vec3 prefilteredColor = textureLod(prefilterMap, R, roughness * maxReflectionLod).rgb;
	vec2 envBRDF = texture(brdfLUT, vec2(NdotV, roughness)).rg;
	vec3 specular = prefilteredColor * (F * envBRDF.x + envBRDF.y);

	vec3 ambient = (kD * diffuse + specular) * ao * envIntensity;
	vec3 color = ambient + Lo;

	FragColor = vec4(color, 1.0);
}
