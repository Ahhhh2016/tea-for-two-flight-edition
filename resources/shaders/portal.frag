#version 330 core

in vec2 v_uv;
out vec4 fragColor;

uniform sampler2D u_portalTex;
uniform float     u_alpha;
uniform float     u_time;

const float PI = 3.141592653589793;

void main() {
    float t = u_time;

    // v_uv: 0..1. Make a centered "screen space" [-1,1] for polar math
    vec2 uvOrig = v_uv;
    vec2 uv     = 2.0 * (uvOrig - 0.5);   // [-1,1] in both axes

    // Polar coords
    float d     = length(uv); // radius
    float ang   = atan(uv.y, uv.x); // -PI..PI

    // fancy irregular radius from https://www.shadertoy.com/view/fddSWj
    float sinVal =
        sin(0.5  + ang * 3.0  + t * 7.0) * sin(ang * 18.0 + t * 2.0) * 0.02
      - cos(0.3  - ang * 8.0  + t * 5.0) * 0.015
      + sin(ang * 8.0  + t * 8.0) * 0.03 * sin(-ang * 2.0 + t * 2.0);

    float r     = 1.0;   // base radius of portal
    float thk   = 0.05;   // thickness band for the edge
    float targetVal = r + sinVal;

    // res ~ "how inside the portal we are" (1 = inside, 0 = outside)
    float res = 1.0 - smoothstep(targetVal - thk, targetVal + thk, d);

    // Completely outside: transparent so background IQ shows through
    if (res <= 0.001) {
        discard; // or fragColor = vec4(0.0);
    }

    // Sample portal interior normally
    vec3 portalColor = texture(u_portalTex, uvOrig).rgb;

    // Edge tint (glow) around the irregular boundary
    vec3 edgeColor = vec3(0.6, 0.9, 1.5); // icy blue
    float edgeBand = smoothstep(targetVal - thk*0.6, targetVal - thk*0.1, d)
                   * (1.0 - smoothstep(targetVal, targetVal + thk*0.8, d));
    // fade out toward the center so only rim glows
    edgeBand *= smoothstep(r - 0.2, r + 0.05, d);

    vec3 col = portalColor + edgeColor * edgeBand;

    fragColor = vec4(col, res * u_alpha);
}
