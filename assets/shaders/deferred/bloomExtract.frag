#version 460 core

layout (location = 0) out vec4 FragColor;

in vec2 uv;

uniform sampler2D hdrScene;
uniform float threshold;

void main()
{
	vec2 texel = 1.0 / vec2(textureSize(hdrScene, 0));
	vec3 color = texture(hdrScene, uv).rgb * 0.5;
	color += texture(hdrScene, uv + vec2(texel.x, 0.0)).rgb * 0.125;
	color += texture(hdrScene, uv - vec2(texel.x, 0.0)).rgb * 0.125;
	color += texture(hdrScene, uv + vec2(0.0, texel.y)).rgb * 0.125;
	color += texture(hdrScene, uv - vec2(0.0, texel.y)).rgb * 0.125;
	float brightness = max(max(color.r, color.g), color.b);
	float contribution = smoothstep(threshold * 0.5, threshold, brightness);
	FragColor = vec4(color * contribution, 1.0);
}
