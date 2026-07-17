#version 460 core

layout (location = 0) out vec4 FragColor;

in vec2 uv;

uniform sampler2D hdrScene;
uniform sampler2D bloomTexture;
uniform sampler2D gPositionAo;
uniform sampler2D gNormalRoughness;
uniform sampler2D gAlbedoMetallic;
uniform sampler2D ssaoTexture;
uniform sampler2D depthTexture;
uniform float exposure;
uniform float bloomIntensity;
uniform float cameraNear;
uniform float cameraFar;
uniform int bloomEnabled;
uniform int toneMapper;
uniform int debugView;

vec3 acesApproximation(vec3 color)
{
	const float a = 2.51;
	const float b = 0.03;
	const float c = 2.43;
	const float d = 0.59;
	const float e = 0.14;
	return clamp((color * (a * color + b)) / (color * (c * color + d) + e), 0.0, 1.0);
}

float linearizeDepth(float depth)
{
	float ndc = depth * 2.0 - 1.0;
	return (2.0 * cameraNear * cameraFar) /
		(cameraFar + cameraNear - ndc * (cameraFar - cameraNear));
}

void main()
{
	if (debugView != 0) {
		vec3 debugColor = vec3(0.0);
		if (debugView == 1) {
			debugColor = fract(abs(texture(gPositionAo, uv).xyz) * 0.1);
		}
		else if (debugView == 2) {
			debugColor = texture(gNormalRoughness, uv).xyz * 0.5 + 0.5;
		}
		else if (debugView == 3) {
			debugColor = pow(max(texture(gAlbedoMetallic, uv).rgb, vec3(0.0)), vec3(1.0 / 2.2));
		}
		else if (debugView == 4) {
			debugColor = vec3(texture(gPositionAo, uv).a);
		}
		else if (debugView == 5) {
			debugColor = vec3(texture(gNormalRoughness, uv).a);
		}
		else if (debugView == 6) {
			debugColor = vec3(texture(gAlbedoMetallic, uv).a);
		}
		else if (debugView == 7) {
			debugColor = vec3(texture(ssaoTexture, uv).r);
		}
		else if (debugView == 8) {
			debugColor = acesApproximation(texture(bloomTexture, uv).rgb * exposure);
		}
		else if (debugView == 9) {
			float depth = linearizeDepth(texture(depthTexture, uv).r) / cameraFar;
			debugColor = vec3(clamp(depth, 0.0, 1.0));
		}
		FragColor = vec4(debugColor, 1.0);
		return;
	}

	vec3 color = texture(hdrScene, uv).rgb;
	if (bloomEnabled != 0) {
		color += texture(bloomTexture, uv).rgb * bloomIntensity;
	}
	color *= exposure;

	if (toneMapper == 0) color = acesApproximation(color);
	else if (toneMapper == 1) color = vec3(1.0) - exp(-color);
	else color = color / (color + vec3(1.0));

	color = pow(max(color, vec3(0.0)), vec3(1.0 / 2.2));
	FragColor = vec4(color, 1.0);
}
