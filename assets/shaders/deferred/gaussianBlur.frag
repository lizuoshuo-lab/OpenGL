#version 460 core

layout (location = 0) out vec4 FragColor;

in vec2 uv;

uniform sampler2D image;
uniform int horizontal;

void main()
{
	const float weights[5] = float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);
	vec2 texel = 1.0 / vec2(textureSize(image, 0));
	vec3 result = texture(image, uv).rgb * weights[0];
	for (int i = 1; i < 5; ++i) {
		vec2 offset = horizontal != 0
			? vec2(texel.x * float(i), 0.0)
			: vec2(0.0, texel.y * float(i));
		result += texture(image, uv + offset).rgb * weights[i];
		result += texture(image, uv - offset).rgb * weights[i];
	}
	FragColor = vec4(result, 1.0);
}
