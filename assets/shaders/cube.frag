#version 460 core
out vec4 FragColor;

in vec3 uvw;

//uniform sampler2D diffuse;
uniform sampler2D sphericalSampler;
uniform float environmentIntensity;
uniform float environmentBlackLevel;
uniform float starIntensity;
uniform float timeSeconds;
uniform float starTwinkleFraction;
uniform float starTwinkleStrength;
uniform float starTwinkleSpeed;
uniform float viewportHeight;

const float PI = 3.1415926535897932384626433832795;

vec2 uvwToUv(vec3 uvwN){
	float phi = asin(uvwN.y);
	float theta = atan(uvwN.z, uvwN.x);
	float u = theta / (2.0 * PI) + 0.5;
	float v = phi / PI + 0.5;
	return vec2(u, v);
}

float hash21(vec2 value)
{
	value = fract(value * vec2(123.34, 456.21));
	value += dot(value, value + 45.32);
	return fract(value.x * value.y);
}

vec3 proceduralStars(vec2 pixelPosition)
{
	float pixelScale = clamp(viewportHeight / 900.0, 1.0, 2.4);
	float cellSize = 9.0 * pixelScale;
	vec2 gridPosition = pixelPosition / cellSize;
	vec2 cell = floor(gridPosition);
	vec2 offset = 0.14 + 0.72 * vec2(
		hash21(cell + vec2(13.0, 7.0)),
		hash21(cell + vec2(31.0, 19.0))
	);
	float seed = hash21(cell + vec2(71.0, 43.0));
	float exists = step(0.972, seed);
	vec2 starCenter = (cell + offset) * cellSize;
	float distanceToStar = length(pixelPosition - starCenter);
	float sizeSeed = hash21(cell + vec2(5.0, 89.0));
	float radius = mix(0.58, 1.18, pow(sizeSeed, 2.4)) * pixelScale;
	float edge = 0.65 * pixelScale;
	float point = 1.0 - smoothstep(radius - edge, radius + edge, distanceToStar);
	float brightness = 0.35 + 1.15 * pow(
		hash21(cell + vec2(109.0, 3.0)),
		3.0
	);
	float temperature = hash21(cell + vec2(17.0, 137.0));
	vec3 warm = vec3(1.0, 0.76, 0.58);
	vec3 neutral = vec3(0.92, 0.95, 1.0);
	vec3 cool = vec3(0.62, 0.77, 1.0);
	vec3 color = temperature < 0.22
		? mix(warm, neutral, temperature / 0.22)
		: mix(neutral, cool, (temperature - 0.22) / 0.78);
	float twinkleSeed = hash21(cell + vec2(191.0, 61.0));
	float twinkleMask = step(1.0 - clamp(starTwinkleFraction, 0.0, 1.0), twinkleSeed);
	float phase = hash21(cell + vec2(43.0, 211.0)) * 2.0 * PI;
	float speedVariation = mix(
		0.72,
		1.28,
		hash21(cell + vec2(157.0, 97.0))
	);
	float twinkleTime = timeSeconds * starTwinkleSpeed * speedVariation;
	float twinkleWave =
		sin(twinkleTime + phase) * 0.72 +
		sin(twinkleTime * 0.43 + phase * 1.73) * 0.28;
	float twinkle = 1.0 + twinkleMask * starTwinkleStrength * twinkleWave;
	return color * point * exists * brightness * max(twinkle, 0.15);
}

void main()
{
	vec3 uvwN = normalize(uvw);
	vec2 uv = uvwToUv(uvwN);
	vec3 environment = texture(sphericalSampler, uv).rgb;
	environment = max(environment - vec3(environmentBlackLevel), vec3(0.0));
	environment *= environmentIntensity;
	environment += proceduralStars(gl_FragCoord.xy) * starIntensity;
	FragColor = vec4(environment, 1.0);
}
