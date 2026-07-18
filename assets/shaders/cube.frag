#version 460 core
out vec4 FragColor;

in vec3 uvw;

//uniform sampler2D diffuse;
uniform sampler2D sphericalSampler;
uniform float environmentIntensity;
uniform float environmentBlackLevel;
uniform float starIntensity;

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

vec3 proceduralStars(vec2 uv)
{
	vec2 gridUv = uv * vec2(1150.0, 575.0);
	vec2 cell = floor(gridUv);
	vec2 local = fract(gridUv) - 0.5;
	vec2 offset = vec2(
		hash21(cell + vec2(13.0, 7.0)),
		hash21(cell + vec2(31.0, 19.0))
	) - 0.5;
	float seed = hash21(cell + vec2(71.0, 43.0));
	float exists = step(0.9935, seed);
	float distanceToStar = length(local - offset * 0.64);
	float sizeSeed = hash21(cell + vec2(5.0, 89.0));
	float radius = mix(0.035, 0.105, pow(sizeSeed, 3.0));
	float edge = max(fwidth(distanceToStar) * 1.15, 0.012);
	float point = 1.0 - smoothstep(radius, radius + edge, distanceToStar);
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
	return color * point * exists * brightness;
}

void main()
{
	vec3 uvwN = normalize(uvw);
	vec2 uv = uvwToUv(uvwN);
	vec3 environment = texture(sphericalSampler, uv).rgb;
	environment = max(environment - vec3(environmentBlackLevel), vec3(0.0));
	environment *= environmentIntensity;
	environment += proceduralStars(uv) * starIntensity;
	FragColor = vec4(environment, 1.0);
}
