#version 330 core
in vec2 v_uv;
out vec4 fragColor;

uniform sampler2D u_colorTex;
uniform sampler2D u_velocityTex;
uniform vec2  u_texelSize;      // 1/width, 1/height
uniform float u_maxBlurPixels;  // clamp in pixels (e.g., 16)
uniform int   u_numSamples;     // total taps along the line (e.g., 12)

void main() {
    vec3 base = texture(u_colorTex, v_uv).rgb;
    vec2 velUV = texture(u_velocityTex, v_uv).xy;    // velocity in UV units (0..1)

    // Convert pixel to UV units
    vec2 maxUV = u_texelSize * u_maxBlurPixels;

    // Clamp velocity magnitude
    vec2 vel = velUV;
    float mag = length(vel);
    float maxMag = length(maxUV);
    if (mag > maxMag && mag > 1e-6) {
        vel *= (maxMag / mag);
        mag = maxMag;
    }

    if (mag < 1e-5 || u_numSamples <= 1) {
        fragColor = vec4(base, 1.0);
        return;
    }

    // Symmetric sampling around current pixel to reduce trailing bias
    int halfSamples = (u_numSamples - 1) / 2;
    vec3 accum = base;
    float wsum = 1.0;

    // Step in UV per sample
    vec2 stepUV = vel / float(max(halfSamples, 1));

    // Linear falloff weights
    for (int i = 1; i <= halfSamples; i++) {
        float t = float(i) / float(halfSamples);
        float w = 1.0 - t; // more weight near the center
        vec2 duv = stepUV * float(i);
        vec3 c1 = texture(u_colorTex, v_uv + duv).rgb;
        vec3 c2 = texture(u_colorTex, v_uv - duv).rgb;
        accum += (c1 + c2) * w;
        wsum += 2.0 * w;
    }

    vec3 color = accum / max(wsum, 1e-6);
    fragColor = vec4(color, 1.0);
}


