// Simple textured quad for portal compositing
#version 330 core
in vec2 v_uv;
out vec4 fragColor;
uniform sampler2D u_portalTex;
uniform float u_alpha; // allow simple fade if desired
void main() {
    vec4 c = texture(u_portalTex, v_uv);
    fragColor = vec4(c.rgb, c.a * u_alpha);
}

