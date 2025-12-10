#version 330 core
in vec2 v_uv;
out vec4 fragColor;

uniform sampler2D u_colorTex;
uniform vec2  u_texelSize;      // 1/width, 1/height
uniform vec2  u_directionUV;    // blur direction in UV units (will be normalized internally)
uniform float u_blurPixels;     // blur half-extent in pixels (e.g., 8-12)
uniform int   u_numSamples;     // odd number recommended (e.g., 11 or  thirteen)

void main() {
    vec3 base = texture(u_colorTex, v_uv).rgb;

    // Convert pixel extent to UV step size
    float maxUVLen = length(u_texelSize * u_blurPixels);

    // Normalize direction, clamp to non-zero
    vec2 dir = u_directionUV;
    float dirLen = length(dir);
    if (dirLen < 1e-5 || u_numSamples <= 1 || u_blurPixels <= 0.0) {
        fragColor = vec4(base, 1.0);
        return;
    }
    dir /= dirLen;
    vec2 stepUV = dir * (maxUVLen / float(max(1, (u_numSamples - 1) / 2)));

    // Accumulate symmetric samples with linear falloff weights
    int halfSamples = (u_numSamples - 1) / 2;
    vec3 accum = base;
    float wsum = 1.0;
    for (int i = 1; i <= halfSamples; i++) {
        float t = float(i) / float(halfSamples);
        float w = 1.0 - t; // heavier weight near center
        vec2 duv = stepUV * float(i);
        vec3 c1 = texture(u_colorTex, v_uv + duv).rgb;
        vec3 c2 = texture(u_colorTex, v_uv - duv).rgb;
        accum += (c1 + c2) * w;
        wsum += 2.0 * w;
    }

    vec3 color = accum / max(wsum, 1e-6);
    fragColor = vec4(color, 1.0);
}


