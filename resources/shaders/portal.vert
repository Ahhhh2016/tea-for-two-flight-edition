// World-space textured quad for portal compositing
#version 330 core
layout (location = 0) in vec3 a_pos; // local XY plane (Z=0)
layout (location = 1) in vec2 a_uv;
uniform mat4 u_M;
uniform mat4 u_V;
uniform mat4 u_P;
out vec2 v_uv;
void main() {
    v_uv = a_uv;
    gl_Position = u_P * u_V * u_M * vec4(a_pos, 1.0);
}

