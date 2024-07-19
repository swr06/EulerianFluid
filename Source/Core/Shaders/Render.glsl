#version 430 core 

layout (location = 0) out vec4 o_Color;

in vec2 v_TexCoords;

uniform float u_zNear;
uniform float u_zFar;

uniform mat4 u_InverseProjection;
uniform mat4 u_InverseView;

vec3 SampleIncidentRayDirection(vec2 screenspace)
{
	vec4 clip = vec4(screenspace * 2.0f - 1.0f, -1.0, 1.0);
	vec4 eye = vec4(vec2(u_InverseProjection * clip), -1.0, 0.0);
	return normalize(vec3(u_InverseView * eye));
}

float LinearizeDepth(float depth)
{
	return (2.0 * u_zNear) / (u_zFar + u_zNear - depth * (u_zFar - u_zNear));
}

vec3 WorldPosFromDepth(float depth, vec2 txc)
{
    float z = depth * 2.0 - 1.0;
    vec4 ClipSpacePosition = vec4(txc * 2.0 - 1.0, z, 1.0);
    vec4 ViewSpacePosition = u_InverseProjection * ClipSpacePosition;
    ViewSpacePosition /= ViewSpacePosition.w;
    vec4 WorldPos = u_InverseView * ViewSpacePosition;
    return WorldPos.xyz;
}

void main() {

	vec3 RayOrigin = u_InverseView[3].xyz;
	vec3 RayDirection = SampleIncidentRayDirection(v_TexCoords);

	o_Color = vec4(v_TexCoords, 1., 1.);
}