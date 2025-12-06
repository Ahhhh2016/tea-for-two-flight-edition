#version 330 core
uniform sampler2D u_depthTex;
in vec2 v_uv;
out vec4 fragColor;

void main() {
    float z = texture(u_depthTex, v_uv).r;
    fragColor = vec4(vec3(z), 1.0);
}


