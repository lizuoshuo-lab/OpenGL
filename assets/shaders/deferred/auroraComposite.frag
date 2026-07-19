#version 460 core

layout (location = 0) out vec4 FragColor;

in vec2 uv;

uniform sampler2D auroraTexture;
uniform sampler2D depthTexture;

void main()
{
	float sceneDepth = texture(depthTexture, uv).r;
	float skyVisibility = step(0.99995, sceneDepth);
	vec3 aurora = texture(auroraTexture, uv).rgb;
	FragColor = vec4(aurora * skyVisibility, 0.0);
}
