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

out vec3 v_n;
out vec3 v_wpos;
out vec2 v_uv;
out vec2 v_velocity;

// object-space position (pre M)
out vec3 v_objPos;

void main() {
    vec4 wpos = u_M * vec4(a_pos, 1.0);
    v_wpos = wpos.xyz;
    v_n = normalize(u_N * a_nor);
    v_uv = a_uv;
    v_objPos = a_pos;
    vec4 clipCurr = u_P * u_V * wpos;
    vec4 clipPrev = u_prevP * u_prevV * (u_prevM * vec4(a_pos, 1.0));
    vec2 ndcCurr = clipCurr.xy / max(clipCurr.w, 1e-6);
    vec2 ndcPrev = clipPrev.xy / max(clipPrev.w, 1e-6);
    vec2 uvCurr = ndcCurr * 0.5 + 0.5;
    vec2 uvPrev = ndcPrev * 0.5 + 0.5;
    v_velocity = uvCurr - uvPrev;
    gl_Position = clipCurr;
}
