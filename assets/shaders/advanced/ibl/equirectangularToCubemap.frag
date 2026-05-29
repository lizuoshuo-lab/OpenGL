#version 460 core
out vec4 FragColor;

in vec3 localPos;

uniform sampler2D equirectangularMap;

const vec2 invAtan = vec2(0.15915494309189535, 0.3183098861837907);

vec2 sampleSphericalMap(vec3 v)
{
	vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
	uv *= invAtan;
	uv += 0.5;
	return uv;
}

void main()
{
	vec2 uv = sampleSphericalMap(normalize(localPos));
	vec3 color = texture(equirectangularMap, uv).rgb;
	FragColor = vec4(color, 1.0);
}
