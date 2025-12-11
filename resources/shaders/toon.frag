#version 330 core

in vec2 v_uv;
out vec4 fragColor;

uniform sampler2D u_sceneTex;
uniform sampler2D u_depthTex;
uniform sampler2D u_normalTex;
uniform sampler2D u_skyTex;

uniform float u_near;
uniform float u_far;

uniform bool u_enablePost;

// Reconstruct view-space Z from non-linear depth
float linearizeDepth(float depth, float nearPlane, float farPlane) {
    // depth is in [0, 1] from the depth buffer
    float z_ndc = depth * 2.0 - 1.0; // back to NDC (-1..1)
    // standard inverse of OpenGL perspective depth mapping
    float z_view = (2.0 * nearPlane * farPlane) /
                   (farPlane + nearPlane - z_ndc * (farPlane - nearPlane));

    // z_view is positive distance from camera
    return z_view;
}

// Read linear view-space depth (in world units, no 0..1 normalization)
float readDepthLinear(sampler2D depthTex, vec2 uv) {
    float d = texture(depthTex, uv).r;
    return linearizeDepth(d, u_near, u_far);
}

vec3 readNormal(vec2 uv) {
    // decode from [0,1] -> [-1,1]
    vec3 enc = texture(u_normalTex, uv).xyz;
    vec3 N = enc * 2.0 - 1.0;
    return normalize(N);
}

void main() {
    // Raw non-linear depth from depth buffer in [0,1]
        float rawDepth = texture(u_depthTex, v_uv).r;

    // If no geometry wrote here (depth == 1), draw sky
    if (rawDepth >= 1.0 - 1e-5) {
        vec2 skyUV = vec2(v_uv.x, 1.0 - v_uv.y);
        vec3 skyColor = texture(u_skyTex, skyUV).rgb;
        fragColor = vec4(skyColor, 1.0);
        return;
    }

    // There *is* geometry here → proceed as before
    vec4 baseColor = texture(u_sceneTex, v_uv);

    if (!u_enablePost) {
        fragColor = baseColor;
        return;
    }
    // Simple toon / posterization
    vec3 color = baseColor.rgb;

    // Number of bands per channel
    float levels = 6.0;

    // Quantize each channel independently
    vec3 toonColor = floor(color * levels) / (levels - 1.0);

    // Blend between original and toon
    float toonAmount = 1.0;
    vec3 shadedColor = mix(color, toonColor, toonAmount);

    // From here on, use shadedColor as the base
    baseColor = vec4(shadedColor, baseColor.a);

    // Shared texel info
    ivec2 texSizeI = textureSize(u_depthTex, 0);
    vec2 texSize   = vec2(texSizeI);
    vec2 texel     = 1.0 / texSize;
    float outlineThickness = 2.0;

    // 1) Depth-based edge
    float d00 = readDepthLinear(u_depthTex, v_uv + outlineThickness * texel * vec2(-1.0,  1.0));
    float d01 = readDepthLinear(u_depthTex, v_uv + outlineThickness * texel * vec2(-1.0,  0.0));
    float d02 = readDepthLinear(u_depthTex, v_uv + outlineThickness * texel * vec2(-1.0, -1.0));

    float d10 = readDepthLinear(u_depthTex, v_uv + outlineThickness * texel * vec2( 0.0,  1.0));
    float d11 = readDepthLinear(u_depthTex, v_uv + outlineThickness * texel * vec2( 0.0,  0.0));
    float d12 = readDepthLinear(u_depthTex, v_uv + outlineThickness * texel * vec2( 0.0, -1.0));

    float d20 = readDepthLinear(u_depthTex, v_uv + outlineThickness * texel * vec2( 1.0,  1.0));
    float d21 = readDepthLinear(u_depthTex, v_uv + outlineThickness * texel * vec2( 1.0,  0.0));
    float d22 = readDepthLinear(u_depthTex, v_uv + outlineThickness * texel * vec2( 1.0, -1.0));

    float gx =
        -1.0 * d00 +  0.0 * d01 +  1.0 * d02 +
        -2.0 * d10 +  0.0 * d11 +  2.0 * d12 +
        -1.0 * d20 +  0.0 * d21 +  1.0 * d22;

    float gy =
        -1.0 * d00 + -2.0 * d01 + -1.0 * d02 +
         0.0 * d10 +  0.0 * d11 +  0.0 * d12 +
         1.0 * d20 +  2.0 * d21 +  1.0 * d22;

    float depthGradient = sqrt(gx * gx + gy * gy);

    float depthEdgeStrength = 0.1;
    float depthEdge = depthGradient * depthEdgeStrength;

    // soft threshold
    depthEdge = smoothstep(0.05, 0.15, depthEdge);

    // 2) Normal-based edge (creases)
    vec3 nCenter = readNormal(v_uv);

    float maxDiff = 0.0;

    // sample a few neighbors
    vec2 offsets[8] = vec2[](
        vec2(-1.0,  0.0),
        vec2( 1.0,  0.0),
        vec2( 0.0, -1.0),
        vec2( 0.0,  1.0),
        vec2(-1.0, -1.0),
        vec2(-1.0,  1.0),
        vec2( 1.0, -1.0),
        vec2( 1.0,  1.0)
    );

    for (int i = 0; i < 8; ++i) {
        vec2 uv = v_uv + outlineThickness * texel * offsets[i];
        vec3 n = readNormal(uv);
        // difference via 1 - dot; 0 if parallel, 1 if orthogonal
        float diff = 1.0 - max(0.0, dot(nCenter, n));
        maxDiff = max(maxDiff, diff);
    }

    // Map normal difference to edge intensity
    float normalEdge = smoothstep(0.1, 0.3, maxDiff);

    // 3) Combine depth + normals
    float outlineMask = clamp(depthEdge + normalEdge, 0.0, 1.0);

    // Final mix
    vec4 outlineColor = vec4(0.0, 0.0, 0.0, 1.0);
    vec4 finalColor   = mix(baseColor, outlineColor, outlineMask);
    fragColor = finalColor;
}
