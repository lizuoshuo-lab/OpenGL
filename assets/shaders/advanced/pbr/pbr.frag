#version 460 core
out vec4 FragColor;

in vec2 uv;
in vec3 normal;
in vec3 worldPosition;
in mat3 tbn;


//相机世界位置
uniform vec3 cameraPosition;

#include "../../common/lightStruct.glsl"


uniform PointLight pointLights[4];
uniform sampler2D albedoTex;	//物体颜色(反照率）

uniform sampler2D normalTex;
uniform sampler2D metallicTex;
uniform sampler2D roughnessTex;
uniform sampler2D aoTex;
uniform samplerCube irradianceMap;

#define PI 3.141592653589793

//NDF：α本应该表示粗糙度（roughness），α= r^2
float NDF_GGX(vec3 N, vec3 H, float roughness){
	float a = roughness * roughness;
	float a2 = a*a;
	float NdotH = max(dot(N,H), 0.0);

	float num = a2;
	float denom =NdotH * NdotH *(a2 - 1.0) + 1.0;
	  denom = PI * denom * denom;
	//denom = max(denom, 0.0001);

	return num / denom;
}

//Geometry
float GeometrySchlickGGX(float NdotV, float roughness){
	float r = (roughness + 1.0);
	float k = r * r / 8.0;

	float num = NdotV;
	float denom = NdotV * (1.0 - k) + k;

	//denom = max(denom, 0.00001);

	return num / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness){
	float NdotV = max(dot(N,V),0.0);
	float NdotL = max(dot(N,L),0.0);

	float ggx1 = GeometrySchlickGGX(NdotV, roughness);
	float ggx2 = GeometrySchlickGGX(NdotL, roughness);

	return ggx1 * ggx2;
}

//Fresnel
vec3 fresnelSchlick(vec3 F0, float HdotV){
	return F0 + (1.0 - F0) * pow((1.0 - HdotV), 5.0);
}

vec3 fresnelSchlickRoughness(vec3 F0,float HdotV, float roughness)
{
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow((1.0 - HdotV), 5.0);
}   
void main()
{
	//1 准备通用数据
	//vec3 albedo  = vec3(0.1,0.8,0.6);
	vec3 albedo  = texture(albedoTex, uv).xyz ;
	float metallic  = texture(metallicTex, uv).b;
    float roughness = texture(roughnessTex, uv).r;
    float ao        = texture(aoTex, uv).g;
    vec3 N = texture(normalTex, uv).rgb;
	N = N * 2.0 - 1.0;
	N = normalize(tbn * N);
	vec3 V = normalize(cameraPosition - worldPosition);
	
	//2 计算基础反射率
	vec3 F0 = vec3(0.04);
	F0 = mix(F0, albedo, metallic);

	//3 遍历四个点光源，计算反射总共的radiance
	vec3 Lo = vec3(0.0);
	for(int i = 0;i <4;i++){
		//3.1 准备计算单个光源贡献的时候用到的通用数据
		vec3 L = normalize(pointLights[i].position - worldPosition);
		vec3 H = normalize(L + V);
		float NdotL = max(dot(N,L),0.0);
		float NdotV = max(dot(N,V),0.0);

		//3.2 计算光源打到平面上的irradiance
		float dis = length(pointLights[i].position - worldPosition);
		float attenuation = 1.0 / (dis * dis);
		vec3 irradiance = pointLights[i].color * NdotL * attenuation; 

		//3.3 计算NDF G F各项的值
		float D = NDF_GGX(N,H,roughness);
		float G = GeometrySmith(N,V,L,roughness);
		vec3 F = fresnelSchlick(F0,max(dot(H,V),0.0));

		//3.4 决定diffuse与specular各自比例多少
		vec3 ks = F;
		vec3 kd = vec3(1.0) - ks;
		//考虑问题：对于金属而言是没有diffuse反射的！
		kd *= (1.0 - metallic);

		//3.5 计算cook-torrance BRDF的值
		vec3 num = D * G * F;
		float denom = max(4.0 * NdotL * NdotV, 0.0000001);
		vec3 specularBRDF = num / denom; 

		//3.6 考虑specular + diffuse最终的反射结果
		Lo += (kd * albedo / PI + specularBRDF) * irradiance;
	}
	//vec3 ambient = vec3(0.03) * albedo*ao;
	
vec3 kd = 1.0 - fresnelSchlickRoughness(F0,max(dot(N,V),0.0), roughness);
vec3 irradiance = texture(irradianceMap, N).rgb;

vec3 ambient    = irradiance * albedo * kd*ao; 

  vec3 color = ambient + Lo;
  //vec3 test=irradiance  * kd*ao; 
	FragColor = vec4(color, 1.0);
}
