#version 330 core
layout(location=0) in vec3 a_pos;

uniform mat4 u_M;
uniform mat4 u_lightViewProj;
uniform float u_time;

uniform bool  u_isMoon;
uniform vec3  u_moonCenter;
uniform float u_orbitSpeed;
uniform float u_orbitPhase;

uniform bool  u_isFloatingCube;
uniform float u_floatSpeed;
uniform float u_floatAmp;
uniform float u_floatPhase;

void main() {
    vec4 wpos = u_M * vec4(a_pos, 1.0);

    // Moon animation
    if (u_isMoon) {
        float ang = u_time * u_orbitSpeed + u_orbitPhase;

        mat2 rot = mat2(
            cos(ang), -sin(ang),
            sin(ang),  cos(ang)
        );

        vec3 rel = wpos.xyz - u_moonCenter;
        vec2 xz  = rot * rel.xz;
        rel.x = xz.x;
        rel.z = xz.y;

        rel.y += 0.1 * sin(ang * 2.0); // same wobble

        wpos.xyz = rel + u_moonCenter;
    }

    // floating cube
    if (u_isFloatingCube) {
        float t   = u_time * u_floatSpeed + u_floatPhase;
        float bob = sin(t) * u_floatAmp;
        wpos.y += bob;
    }

    gl_Position = u_lightViewProj * wpos;
}
