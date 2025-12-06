#version 330 core
in vec2 v_uv;
out vec4 fragColor;

uniform sampler2D u_colorTex;
uniform sampler2D u_depthTex;

uniform float u_near;
uniform float u_far;
uniform float u_focusDist;     // world units from camera
uniform float u_focusRange;    // +/- range around focus distance that remains sharp
uniform float u_maxBlurRadius; // in pixels
uniform int   u_enable;        // 0/1
uniform vec2  u_texelSize;     // 1/width, 1/height

float linearizeDepth(float depth)
{
    // perspective: depth is in [0,1], linearize using near/far
    float z = depth * 2.0 - 1.0; // back to NDC
    float n = u_near;
    float f = u_far;
    return (2.0 * n * f) / (f + n - z * (f - n));
}

void main() {
    vec3 base = texture(u_colorTex, v_uv).rgb;
    if (u_enable == 0) {
        fragColor = vec4(base, 1.0);
        return;
    }

    float depthSample = texture(u_depthTex, v_uv).r;
    float linearDepth = linearizeDepth(depthSample);

    // Circle of confusion based on distance from focus plane
    float distFromFocus = abs(linearDepth - u_focusDist);
    float coc = clamp((distFromFocus - u_focusRange) / max(u_focusRange, 1e-4), 0.0, 1.0);

    // Convert CoC to blur radius in pixels
    float radius = coc * u_maxBlurRadius;
    if (radius < 0.5) {
        fragColor = vec4(base, 1.0);
        return;
    }

    // Perform 12 Poisson disk samplings for blurring.
    vec2 offsets[12] = vec2[](
        vec2( 0.0,  1.0),
        vec2( 0.87, 0.5),
        vec2( 0.87,-0.5),
        vec2( 0.0, -1.0),
        vec2(-0.87,-0.5),
        vec2(-0.87, 0.5),
        vec2( 0.5,  0.87),
        vec2( 1.0,  0.0),
        vec2( 0.5, -0.87),
        vec2(-0.5, -0.87),
        vec2(-1.0,  0.0),
        vec2(-0.5,  0.87)
    );

    vec3 accum = base;
    float wsum = 1.0;
    for (int i = 0; i < 12; i++) {
        vec2 duv = offsets[i] * (radius * u_texelSize);
        vec3 c = texture(u_colorTex, v_uv + duv).rgb;
        // weight falloff based on distance
        float r2 = dot(duv, duv);
        float sigma = max(radius * 0.5, 1e-3);
        float w = exp(-r2 / (2.0 * sigma * sigma)); 

        accum += c * w;
        wsum += w;
    }
    vec3 blurred = accum / max(wsum, 1e-4);

    // Blend based on CoC
    vec3 color = mix(base, blurred, clamp(coc, 0.0, 1.0));
    fragColor = vec4(color, 1.0);
}


