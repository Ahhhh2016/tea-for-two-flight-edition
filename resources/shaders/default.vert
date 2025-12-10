#version 330 core
layout(location=0) in vec3 a_pos;
layout(location=1) in vec3 a_nor;
layout(location=2) in vec2 a_uv;

uniform mat4 u_M;
uniform mat4 u_V;
uniform mat4 u_P;
uniform mat3 u_N;
uniform mat4 u_prevM;
uniform mat4 u_prevV;
uniform mat4 u_prevP;

uniform float u_time;

uniform bool  u_isMoon;
uniform vec3  u_moonCenter;
uniform float u_orbitSpeed;
uniform float u_orbitPhase;

uniform bool  u_isFloatingCube;
uniform float u_floatSpeed;
uniform float u_floatAmp;
uniform float u_floatPhase;

uniform mat4 u_lightViewProj;

out vec3 v_n;
out vec3 v_wpos;
out vec2 v_uv;
out vec2 v_velocity;
out vec3 v_objPos;
out vec4 v_lightSpacePos;

void main() {
    // start in world space
    vec4 wpos = u_M * vec4(a_pos, 1.0);

    // Orbiting moons around a center
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

        // tiny vertical wobble so it feels alive
        rel.y += 0.1 * sin(ang * 2.0);

        wpos.xyz = rel + u_moonCenter;
    }

    // Bobbing cubes up and down
    if (u_isFloatingCube) {
        float t   = u_time * u_floatSpeed + u_floatPhase;
        float bob = sin(t) * u_floatAmp;
        wpos.y += bob;
    }

    v_wpos   = wpos.xyz;
    v_n      = normalize(u_N * a_nor);
    v_uv     = a_uv;
    v_objPos = a_pos;

    vec4 clipCurr = u_P * u_V * wpos;

    // previous-frame position for motion blur (uses prev matrices)
    vec4 prevWorldPos = u_prevM * vec4(a_pos, 1.0);
    vec4 clipPrev     = u_prevP * u_prevV * prevWorldPos;

    vec2 ndcCurr = clipCurr.xy / max(clipCurr.w, 1e-6);
    vec2 ndcPrev = clipPrev.xy / max(clipPrev.w, 1e-6);

    vec2 uvCurr = ndcCurr * 0.5 + 0.5;
    vec2 uvPrev = ndcPrev * 0.5 + 0.5;

    v_velocity = uvCurr - uvPrev;

    gl_Position = clipCurr;
    v_lightSpacePos = u_lightViewProj * wpos;
}
