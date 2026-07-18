#version 460 core

layout (location = 0) out float FragOcclusion;

in vec2 uv;

uniform sampler2D ssaoInput;
uniform sampler2D gPositionAo;
uniform sampler2D gNormalRoughness;

void main()
{
	vec2 texel = 1.0 / vec2(textureSize(ssaoInput, 0));
	vec3 centerPosition = texture(gPositionAo, uv).xyz;
	vec3 centerNormal = texture(gNormalRoughness, uv).xyz;
	if (length(centerNormal) < 0.1) {
		FragOcclusion = 1.0;
		return;
	}
	centerNormal = normalize(centerNormal);
	float weightedOcclusion = 0.0;
	float totalWeight = 0.0;

	for (int y = -3; y <= 3; ++y) {
		for (int x = -3; x <= 3; ++x) {
			vec2 sampleUv = uv + vec2(x, y) * texel;
			vec3 samplePosition = texture(gPositionAo, sampleUv).xyz;
			vec3 sampleNormal = texture(gNormalRoughness, sampleUv).xyz;
			if (length(sampleNormal) < 0.1) {
				continue;
			}
			sampleNormal = normalize(sampleNormal);
			vec3 positionDelta = samplePosition - centerPosition;
			float spatialWeight = exp(-0.20 * float(x * x + y * y));
			float planeWeight = exp(-12.0 * abs(dot(positionDelta, centerNormal)));
			float normalWeight = pow(max(dot(centerNormal, sampleNormal), 0.0), 8.0);
			float weight = spatialWeight * planeWeight * normalWeight;
			weightedOcclusion += texture(ssaoInput, sampleUv).r * weight;
			totalWeight += weight;
		}
	}

	FragOcclusion = weightedOcclusion / max(totalWeight, 0.0001);
}
