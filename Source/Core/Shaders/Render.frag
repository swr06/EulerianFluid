#version 450 core 


layout (location = 0) out vec4 o_Color;

in vec2 v_TexCoords;

uniform int u_Resolution;


layout (std430, binding = 0) buffer SSBO_HM {
	float PressureGradient[];
};

int To1DIdx(int x, int y) {
	return (y * u_Resolution) + x;
}

float CatmullRom(in vec2 uv);
float Bicubic(vec2 coord);

float Sample(vec2 UV) {
	ivec2 Texel = ivec2((UV) * vec2(float(u_Resolution)));
	return PressureGradient[To1DIdx(Texel.x, Texel.y)];
}

float Sample(ivec2 px) {
    
    px = clamp(px, ivec2(0), ivec2(u_Resolution - 1));
	return PressureGradient[To1DIdx(px.x, px.y)];
}

float Bilinear(vec2 SampleUV)
{
    const ivec2 Cross[4] = ivec2[4](ivec2(0, 0), ivec2(1, 0), ivec2(0, 1), ivec2(1, 1));

    // Relative to center
    vec2 SamplingFragment = (SampleUV * vec2(u_Resolution)) - vec2(0.5f); 

    // Find how much you need to interpolate across both axis
    vec2 f = fract(SamplingFragment);

    // Fetch 4 neighbours
    float Fetch[4];
    Fetch[0] = Sample(ivec2(SamplingFragment) + Cross[0]);
    Fetch[1] = Sample(ivec2(SamplingFragment) + Cross[1]);
    Fetch[2] = Sample(ivec2(SamplingFragment) + Cross[2]);
    Fetch[3] = Sample(ivec2(SamplingFragment) + Cross[3]);

    // Interpolate first based on x position
    float Interp1 = mix(Fetch[0], Fetch[1], float(f.x));
    float Interp2 = mix(Fetch[2], Fetch[3], float(f.x));

    return mix(Interp1, Interp2, float(f.y));
}

float SampleHeight(vec2 UV){
    return Bilinear(UV);
}

void main() {


	o_Color = vec4(vec3(Bilinear(v_TexCoords)),1.);
}