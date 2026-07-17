#version 460 core

layout (location = 0) out float FragOcclusion;

in vec2 uv;

uniform sampler2D ssaoInput;
uniform sampler2D gPositionAo;

void main()
{
	vec2 texel = 1.0 / vec2(textureSize(ssaoInput, 0));
	vec3 centerPosition = texture(gPositionAo, uv).xyz;
	float weightedOcclusion = 0.0;
	float totalWeight = 0.0;

	for (int y = -2; y <= 2; ++y) {
		for (int x = -2; x <= 2; ++x) {
			vec2 sampleUv = uv + vec2(x, y) * texel;
			vec3 samplePosition = texture(gPositionAo, sampleUv).xyz;
			float spatialWeight = exp(-0.35 * float(x * x + y * y));
			float depthWeight = exp(-4.0 * length(samplePosition - centerPosition));
			float weight = spatialWeight * depthWeight;
			weightedOcclusion += texture(ssaoInput, sampleUv).r * weight;
			totalWeight += weight;
		}
	}

	FragOcclusion = weightedOcclusion / max(totalWeight, 0.0001);
}
