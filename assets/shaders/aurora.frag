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
uniform float bandVariation;
uniform float rayDetail;
uniform float diffuseSkyIntensity;
uniform float redEmission;
uniform float blueEmission;
uniform int raymarchSteps;
uniform vec3 greenColor;
uniform vec3 redColor;
uniform vec3 blueColor;

const int MAX_RAYMARCH_STEPS = 96;
const int CURTAIN_COUNT = 3;

float hash11(float value)
{
	value = fract(value * 0.1031);
	value *= value + 33.33;
	value *= value + value;
	return fract(value);
}

float hash21(vec2 value)
{
	vec3 p3 = fract(vec3(value.xyx) * 0.1031);
	p3 += dot(p3, p3.yzx + 33.33);
	return fract((p3.x + p3.y) * p3.z);
}

float smoothNoise1(float value)
{
	float cell = floor(value);
	float fraction = fract(value);
	float blend = fraction * fraction * (3.0 - 2.0 * fraction);
	return mix(hash11(cell), hash11(cell + 1.0), blend);
}

float smoothNoise2(vec2 value)
{
	vec2 cell = floor(value);
	vec2 fraction = fract(value);
	vec2 blend = fraction * fraction * (3.0 - 2.0 * fraction);
	float bottom = mix(hash21(cell), hash21(cell + vec2(1.0, 0.0)), blend.x);
	float top = mix(
		hash21(cell + vec2(0.0, 1.0)),
		hash21(cell + vec2(1.0, 1.0)),
		blend.x
	);
	return mix(bottom, top, blend.y);
}

float fbm2(vec2 value)
{
	float result = 0.0;
	float amplitude = 0.58;
	mat2 rotation = mat2(0.80, -0.60, 0.60, 0.80);
	for (int octave = 0; octave < 3; ++octave) {
		result += smoothNoise2(value) * amplitude;
		value = rotation * value * 2.03 + vec2(7.1, 3.7);
		amplitude *= 0.48;
	}
	return result;
}

float gaussian(float value, float center, float sigma)
{
	float normalized = (value - center) / max(sigma, 0.0001);
	return exp(-0.5 * normalized * normalized);
}

float curtainCenter(
	vec3 position,
	float normalizedHeight,
	float layer,
	float motion,
	float macroWarp
)
{
	float phase = layer * 2.73;
	float altitude = normalizedHeight - 0.5;
	float orientation = (hash11(layer * 17.1 + 2.0) - 0.5) * 0.42;
	float warpedX = position.x + macroWarp * foldStrength * 1.85 +
		altitude * foldStrength * (0.72 + 0.38 * sin(position.x * 0.027 + phase));
	float broadFold = sin(warpedX * foldScale + phase + motion * 0.22);
	float sweepingFold = sin(
		warpedX * foldScale * 0.47 + altitude * 4.2 + phase * 1.41 - motion * 0.14
	);
	float localCurl = sin(
		warpedX * foldScale * 2.63 - altitude * 2.1 + phase * 0.68 + motion * 0.31
	);
	float fold = broadFold * 0.62 + sweepingFold * 0.31 +
		localCurl * turbulence * 0.14;
	float heightSweep = altitude * foldStrength * 0.42 *
		sin(position.x * 0.031 + phase * 1.8 - motion * 0.11);
	return -curtainDistance - layer * curtainSpread + orientation * position.x +
		fold * foldStrength + heightSweep;
}

vec2 curtainField(vec3 position, float motion)
{
	float normalizedHeight = clamp(
		(position.y - lowerHeight) / max(upperHeight - lowerHeight, 0.001),
		0.0,
		1.0
	);
	vec2 flowCoordinate = vec2(
		position.x * 0.026 + motion * 0.050,
		position.y * 0.034 - motion * 0.034
	);
	float flowA = fbm2(flowCoordinate);
	float flowB = fbm2(
		flowCoordinate * 0.61 + vec2(8.7, -4.1) - vec2(motion * 0.032, motion * 0.021)
	);
	float macroWarp = (flowA - 0.5) * 2.0 +
		0.42 * sin(flowB * 6.2831853 + normalizedHeight * 3.1);
	float marbleBand = 1.0 - smoothstep(0.08, 0.48, abs(flowA - flowB));
	float total = 0.0;
	float nearestDistance = 1000.0;
	for (int layerIndex = 0; layerIndex < CURTAIN_COUNT; ++layerIndex) {
		float layer = float(layerIndex);
		float phase = layer * 2.73;
		float center = curtainCenter(
			position,
			normalizedHeight,
			layer,
			motion,
			macroWarp
		);
		float distanceToSheet = abs(position.z - center);
		nearestDistance = min(nearestDistance, distanceToSheet);
		float thickness = curtainThickness * mix(0.78, 1.38, flowB) *
			mix(0.92, 1.12, layer * 0.5);
		float sheetCore = exp(-pow(distanceToSheet / max(thickness, 0.05), 2.0));

		float spanOffset = (layer - 1.0) * 10.0;
		float span = 1.0 - smoothstep(
			curtainSpan * 0.66,
			curtainSpan,
			abs(position.x + spanOffset)
		);

		float patchNoise = 0.5 + 0.5 * sin(
			flowA * 5.7 + flowB * 3.2 + position.x * 0.022 + phase - motion * 0.17
		);
		float bandSignal = marbleBand * 0.64 + patchNoise * 0.36;
		float brokenBand = smoothstep(0.42, 0.72, bandSignal);
		float broadPatch = mix(1.0, 0.06 + 0.94 * brokenBand, bandVariation);

		float pathNoise = fbm2(vec2(
			position.x * 0.017 + layer * 3.7 - motion * 0.028,
			layer * 5.3 + position.z * 0.004 + motion * 0.010
		));
		float pathDetail = fbm2(vec2(
			position.x * 0.041 - layer * 2.1 + motion * 0.019,
			layer * 8.7 + 2.4
		));
		float directionalSweep = sin(layer * 4.13 + 0.70) * 0.13 *
			(position.x / max(curtainSpan, 1.0));
		float ribbonCenter = 0.31 + layer * 0.18 + directionalSweep +
			(pathNoise - 0.52) * 0.30 +
			0.055 * sin(position.x * 0.029 + phase - motion * 0.047) +
			(pathDetail - 0.5) * 0.055;
		ribbonCenter = clamp(ribbonCenter, 0.17, 0.88);
		float ribbonWidth = mix(0.036, 0.064, flowB) *
			mix(1.08, 0.84, layer * 0.5);
		float heightOffset = normalizedHeight - ribbonCenter;
		float lowerDistance = min(heightOffset, 0.0) /
			max(ribbonWidth * 0.58, 0.01);
		float sharpLowerHalf = exp(-lowerDistance * lowerDistance);
		float softUpperHalf = exp(-pow(
			max(heightOffset, 0.0) / max(ribbonWidth * 1.12, 0.01),
			1.45
		));
		float ribbonRidge = sharpLowerHalf * softUpperHalf;
		float foldOffset = ribbonWidth * (
			1.42 + 0.48 * sin(position.x * 0.072 + phase * 1.7 - motion * 0.078)
		);
		float foldedDistance = (heightOffset - foldOffset) /
			max(ribbonWidth * 0.38, 0.01);
		float foldedRidge = exp(-foldedDistance * foldedDistance);

		float rayCoordinate = position.x * (0.19 + layer * 0.018) +
			normalizedHeight * (1.2 + 0.7 * sin(position.x * 0.021 + phase)) +
			layer * 11.0 + motion * 0.28;
		float rayNoise = smoothNoise1(rayCoordinate);
		float rays = mix(0.34, 1.18, pow(rayNoise, 2.4));
		float rayModulation = mix(1.0, rays, rayDetail);
		float verticalProfile = ribbonRidge * (0.84 + 0.12 * rayModulation) +
			foldedRidge * (0.07 + 0.055 * marbleBand);
		float ribbon = sheetCore * verticalProfile;
		total += ribbon * span * broadPatch * (1.0 - layer * 0.22);
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

float diffuseSkyField(vec3 rayDirection, float motion)
{
	float azimuth = atan(rayDirection.x, -rayDirection.z);
	float elevation = asin(clamp(rayDirection.y, -1.0, 1.0));
	vec2 skyCoordinate = vec2(azimuth * 1.15, elevation * 2.35);
	float domainWarp = fbm2(vec2(
		skyCoordinate.x * 0.58 + motion * 0.080,
		skyCoordinate.y * 0.42 - motion * 0.045
	));
	vec2 veilCoordinate = vec2(
		(skyCoordinate.x + (domainWarp - 0.5) * 0.82) * 1.85 + motion * 0.050,
		skyCoordinate.y * 0.34 - motion * 0.025
	);
	float broadVeil = fbm2(veilCoordinate);
	float verticalDrape = fbm2(vec2(
		veilCoordinate.x * 2.45 - motion * 0.12 + 4.7,
		veilCoordinate.y * 0.30 + domainWarp * 0.65
	));
	float foldedVein = 1.0 - smoothstep(
		0.07,
		0.30,
		abs(broadVeil - verticalDrape)
	);
	float broadMask = smoothstep(0.44, 0.68, broadVeil);
	float drapeMask = smoothstep(0.46, 0.68, verticalDrape);
	float columnCoordinate = skyCoordinate.x * 3.8 +
		(domainWarp - 0.5) * 2.2 +
		sin(skyCoordinate.y * 1.2 - motion * 0.060) * 0.9 -
		motion * 0.18;
	float columnMask = smoothstep(0.40, 0.74, smoothNoise1(columnCoordinate));
	float veil = broadMask * 0.30 + drapeMask * 0.24 +
		columnMask * 0.38 + foldedVein * 0.08;
	float horizonLift = exp(-max(elevation, 0.0) * 1.55);
	return (0.16 + veil * 0.84) *
		(0.64 + horizonLift * 0.36);
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
		float flux = field.x * smoothstep(0.018, 0.095, field.x);
		float adaptiveStep = clamp(
			field.y * 0.32 / max(abs(rayDirection.z), 0.18),
			minimumStep,
			maximumStep
		);
		if (flux > 0.003) {
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
	float skyVisibility = smoothstep(0.006, 0.032, rayDirection.y);
	float skyVeil = diffuseSkyField(rayDirection, motion);
	vec3 skyTint = mix(vec3(0.008, 0.075, 0.085), greenColor * 0.15, 0.34);
	vec3 diffuseSky = skyTint * skyVeil * diffuseSkyIntensity * skyVisibility;
	FragColor = vec4(accumulated * intensity * horizonFade + diffuseSky, 0.0);
}
