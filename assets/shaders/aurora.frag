#version 460 core

layout (location = 0) out vec4 FragColor;

in vec3 worldRay;

uniform vec3 cameraPosition;
uniform float timeSeconds;
uniform float intensity;
uniform float motionSpeed;
uniform float lowerHeight;
uniform float upperHeight;
uniform float curtainDistance;
uniform float curtainSpread;
uniform float curtainSpan;
uniform float curtainThickness;
uniform float foldScale;
uniform float foldStrength;
uniform float turbulence;
uniform float redEmission;
uniform float blueEmission;
uniform int raymarchSteps;
uniform vec3 greenColor;
uniform vec3 redColor;
uniform vec3 blueColor;

const int MAX_RAYMARCH_STEPS = 96;
const int CURTAIN_COUNT = 4;

float hash11(float value)
{
	value = fract(value * 0.1031);
	value *= value + 33.33;
	value *= value + value;
	return fract(value);
}

float smoothNoise1(float value)
{
	float cell = floor(value);
	float fraction = fract(value);
	float blend = fraction * fraction * (3.0 - 2.0 * fraction);
	return mix(hash11(cell), hash11(cell + 1.0), blend);
}

float gaussian(float value, float center, float sigma)
{
	float normalized = (value - center) / max(sigma, 0.0001);
	return exp(-0.5 * normalized * normalized);
}

float curtainCenter(float x, float layer, float motion)
{
	float phase = layer * 2.31;
	float broadFold = sin(x * foldScale + phase + motion * 0.21);
	float middleFold = sin(x * foldScale * 2.17 - phase * 1.37 - motion * 0.13);
	float fineFold = sin(x * foldScale * 5.43 + phase * 0.73 + motion * 0.31);
	float fold = broadFold + 0.38 * middleFold + turbulence * 0.18 * fineFold;
	return -curtainDistance - layer * curtainSpread + fold * foldStrength;
}

vec2 curtainField(vec3 position, float motion)
{
	float total = 0.0;
	float nearestDistance = 1000.0;
	for (int layerIndex = 0; layerIndex < CURTAIN_COUNT; ++layerIndex) {
		float layer = float(layerIndex);
		float center = curtainCenter(position.x, layer, motion);
		float distanceToSheet = abs(position.z - center);
		nearestDistance = min(nearestDistance, distanceToSheet);
		float thickness = curtainThickness * mix(0.82, 1.25, layer / 3.0);
		float sheet = exp(-pow(distanceToSheet / max(thickness, 0.05), 2.0));

		float spanOffset = (layer - 1.5) * 7.0;
		float span = 1.0 - smoothstep(
			curtainSpan * 0.72,
			curtainSpan,
			abs(position.x + spanOffset)
		);
		float rayCoordinate = position.x * (1.18 + layer * 0.13) + layer * 17.0;
		float rayNoise = smoothNoise1(rayCoordinate + motion * (0.42 + layer * 0.07));
		float filament = mix(0.16, 1.0, pow(rayNoise, 2.8));
		float pulse = 0.78 + 0.22 * sin(position.x * 0.19 - motion * 0.55 + layer * 1.7);
		total += sheet * span * filament * pulse * (1.0 - layer * 0.12);
	}
	return vec2(total, nearestDistance);
}

vec3 heightEmission(float normalizedHeight)
{
	float greenLayer = gaussian(normalizedHeight, 0.31, 0.18) +
		0.32 * gaussian(normalizedHeight, 0.52, 0.25);
	float redLayer = gaussian(normalizedHeight, 0.83, 0.23) * redEmission;
	float blueLayer = gaussian(normalizedHeight, 0.055, 0.052) * blueEmission;
	float lowerEdge = smoothstep(0.0, 0.025, normalizedHeight);
	float upperEdge = 1.0 - smoothstep(0.96, 1.0, normalizedHeight);
	return (greenColor * greenLayer + redColor * redLayer + blueColor * blueLayer) *
		lowerEdge * upperEdge;
}

void main()
{
	vec3 rayDirection = normalize(worldRay);
	if (rayDirection.y <= 0.012 || upperHeight <= lowerHeight) {
		FragColor = vec4(0.0);
		return;
	}

	float startDistance = (lowerHeight - cameraPosition.y) / rayDirection.y;
	float endDistance = (upperHeight - cameraPosition.y) / rayDirection.y;
	if (startDistance > endDistance) {
		float swapDistance = startDistance;
		startDistance = endDistance;
		endDistance = swapDistance;
	}
	startDistance = max(startDistance, 0.0);
	endDistance = min(endDistance, 190.0);
	if (endDistance <= startDistance) {
		FragColor = vec4(0.0);
		return;
	}

	int steps = clamp(raymarchSteps, 16, MAX_RAYMARCH_STEPS);
	float nominalStep = (endDistance - startDistance) / float(steps);
	float minimumStep = max(nominalStep * 0.24, 0.20);
	float maximumStep = max(nominalStep * 3.5, 2.0);
	float motion = timeSeconds * motionSpeed;
	vec3 accumulated = vec3(0.0);
	float transmittance = 1.0;
	float sampleDistance = startDistance + minimumStep * 0.5;

	for (int stepIndex = 0; stepIndex < MAX_RAYMARCH_STEPS; ++stepIndex) {
		if (stepIndex >= steps || sampleDistance >= endDistance) {
			break;
		}
		vec3 position = cameraPosition + rayDirection * sampleDistance;
		vec2 field = curtainField(position, motion);
		float flux = field.x;
		float adaptiveStep = clamp(
			field.y * 0.32 / max(abs(rayDirection.z), 0.18),
			minimumStep,
			maximumStep
		);
		if (flux > 0.001) {
			float normalizedHeight = (position.y - lowerHeight) /
				max(upperHeight - lowerHeight, 0.001);
			float sampleDensity = flux * adaptiveStep * 0.12;
			vec3 emission = heightEmission(normalizedHeight) * flux;
			accumulated += transmittance * emission * adaptiveStep * 0.095;
			transmittance *= exp(-sampleDensity * 0.055);
		}
		sampleDistance += adaptiveStep;
	}

	float horizonFade = smoothstep(0.012, 0.07, rayDirection.y);
	FragColor = vec4(accumulated * intensity * horizonFade, 0.0);
}
