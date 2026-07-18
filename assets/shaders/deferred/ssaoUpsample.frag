#version 460 core

layout (location = 0) out float FragOcclusion;

in vec2 uv;

uniform sampler2D ssaoInput;
uniform sampler2D gPositionAo;
uniform sampler2D gNormalRoughness;

void main()
{
	vec3 centerPosition = texture(gPositionAo, uv).xyz;
	vec3 centerNormal = texture(gNormalRoughness, uv).xyz;
	if (length(centerNormal) < 0.1) {
		FragOcclusion = 1.0;
		return;
	}
	centerNormal = normalize(centerNormal);

	ivec2 sourceSize = textureSize(ssaoInput, 0);
	vec2 sourcePixel = uv * vec2(sourceSize) - 0.5;
	ivec2 basePixel = ivec2(floor(sourcePixel));
	vec2 fraction = fract(sourcePixel);
	float weightedOcclusion = 0.0;
	float totalWeight = 0.0;

	for (int y = 0; y <= 1; ++y) {
		for (int x = 0; x <= 1; ++x) {
			ivec2 sourceCoordinate = clamp(
				basePixel + ivec2(x, y),
				ivec2(0),
				sourceSize - ivec2(1)
			);
			vec2 sampleUv = (vec2(sourceCoordinate) + 0.5) / vec2(sourceSize);
			vec3 sampleNormal = texture(gNormalRoughness, sampleUv).xyz;
			if (length(sampleNormal) < 0.1) {
				continue;
			}
			sampleNormal = normalize(sampleNormal);
			vec3 samplePosition = texture(gPositionAo, sampleUv).xyz;
			float bilinearWeight =
				(x == 0 ? 1.0 - fraction.x : fraction.x) *
				(y == 0 ? 1.0 - fraction.y : fraction.y);
			float planeWeight = exp(
				-16.0 * abs(dot(samplePosition - centerPosition, centerNormal))
			);
			float normalWeight = pow(
				max(dot(centerNormal, sampleNormal), 0.0),
				12.0
			);
			float weight = bilinearWeight * planeWeight * normalWeight;
			weightedOcclusion += texelFetch(ssaoInput, sourceCoordinate, 0).r * weight;
			totalWeight += weight;
		}
	}

	FragOcclusion = totalWeight > 0.0001
		? weightedOcclusion / totalWeight
		: texture(ssaoInput, uv).r;
}
