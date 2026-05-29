#include "lightStruct.glsl"

vec3 calculateDiffuse(vec3 lightColor, vec3 objectColor, vec3 lightDir, vec3 normal)
{
	float diffuse = clamp(dot(-lightDir, normal), 0.0, 1.0);
	return lightColor * diffuse * objectColor;
}

vec3 calculateSpecular(vec3 lightColor, vec3 lightDir, vec3 normal, vec3 viewDir, float intensity)
{
	float dotResult = dot(-lightDir, normal);
	float flag = step(0.0, dotResult);
	vec3 lightReflect = normalize(reflect(lightDir, normal));
	float specular = max(dot(lightReflect, -viewDir), 0.0);
	specular = pow(specular, shiness);
	return lightColor * specular * flag * intensity;
}

vec3 calculateDirectionalLight(vec3 objectColor, DirectionalLight light, vec3 normal, vec3 viewDir)
{
	light.color *= light.intensity;
	vec3 lightDir = normalize(light.direction);
	vec3 diffuseColor = calculateDiffuse(light.color, objectColor, lightDir, normal);
	vec3 specularColor = calculateSpecular(light.color, lightDir, normal, viewDir, light.specularIntensity);
	return diffuseColor + specularColor;
}

vec3 calculatePointLight(vec3 objectColor, PointLight light, vec3 normal, vec3 viewDir)
{
	vec3 lightDir = normalize(worldPosition - light.position);
	float dist = length(worldPosition - light.position);
	float attenuation = 1.0 / (light.k2 * dist * dist + light.k1 * dist + light.kc);
	vec3 diffuseColor = calculateDiffuse(light.color, objectColor, lightDir, normal);
	vec3 specularColor = calculateSpecular(light.color, lightDir, normal, viewDir, light.specularIntensity);
	return (diffuseColor + specularColor) * attenuation;
}

vec3 calculateSpotLight(vec3 objectColor, SpotLight light, vec3 normal, vec3 viewDir)
{
	vec3 lightDir = normalize(worldPosition - light.position);
	vec3 targetDir = normalize(light.targetDirection);
	float cGamma = dot(lightDir, targetDir);
	float intensity = clamp((cGamma - light.outerLine) / (light.innerLine - light.outerLine), 0.0, 1.0);
	vec3 diffuseColor = calculateDiffuse(light.color, objectColor, lightDir, normal);
	vec3 specularColor = calculateSpecular(light.color, lightDir, normal, viewDir, light.specularIntensity);
	return (diffuseColor + specularColor) * intensity;
}
