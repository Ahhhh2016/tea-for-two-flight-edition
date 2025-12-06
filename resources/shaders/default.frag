#version 330 core
in vec3 v_n;
in vec3 v_wpos;
in vec2 v_uv;
in vec2 v_velocity;

const int MAX_LIGHTS = 8;

uniform int  u_numLights;
uniform int  u_lightType[MAX_LIGHTS];   // 0=directional, 1=point, 2=spot
uniform vec3 u_lightColor[MAX_LIGHTS];
uniform vec3 u_lightPos[MAX_LIGHTS];    // unused for directional
uniform vec3 u_lightDir[MAX_LIGHTS];    // unused for point
uniform vec3 u_lightFunc[MAX_LIGHTS];   // attenuation (c, l, q)
uniform float u_lightAngle[MAX_LIGHTS]; // radians
uniform float u_lightPenumbra[MAX_LIGHTS]; // radians

uniform vec3 u_global; // (ka, kd, ks)

uniform vec3 u_ka;
uniform vec3 u_kd;
uniform vec3 u_ks;
uniform float u_shininess;
uniform vec3 u_camPos;

// Atmospheric fog
uniform vec3 u_fogColor;
uniform float u_fogDensity; // use exp2 fog: factor = 1 - exp(-(density * dist)^2)
uniform int u_fogEnable;

// Texture mapping
uniform sampler2D u_tex;
uniform int  u_hasTexture;
uniform vec2 u_texRepeat;
uniform float u_blend; // 0 = use pure u_kd; 1 = use pure texture

layout(location=0) out vec4 fragColor;
layout(location=1) out vec2 outVelocity;

void main() {
    vec3 n = normalize(v_n);
    vec3 V = normalize(u_camPos - v_wpos);

    float b = clamp(u_blend, 0.0, 1.0);
    vec3 baseKd;
    if (u_hasTexture == 1) {
        vec3 texColor = texture(u_tex, v_uv * u_texRepeat).rgb;
        baseKd = u_global.y * u_kd * (1.0 - b) + texColor * b;
    } else {
        baseKd = u_global.y * u_kd;
    }

    vec3 color = u_global.x * u_ka; // ambient (leave as material ambient)

    for (int i = 0; i < u_numLights; i++) {
        vec3 L;
        float attenuation = 1.0;
        float spotFactor = 1.0;

        if (u_lightType[i] == 0) {
            // Directional
            vec3 dir = normalize(u_lightDir[i]);
            L = -dir;
        } else {
            // Point or Spot
            vec3 toLight = u_lightPos[i] - v_wpos;
            float dist = length(toLight);
            if (dist > 0.0) {
                L = toLight / dist;
            } else {
                L = vec3(0.0, 1.0, 0.0);
            }
            vec3 f = u_lightFunc[i];
            attenuation = 1.0 / max(f.x + f.y * dist + f.z * dist * dist, 0.0001);

            if (u_lightType[i] == 2) {
                // Spot: falloff following formula
                vec3 spotDir = normalize(u_lightDir[i]);
                // outer cone = angle, inner = angle - penumbra
                float innerC = cos(max(u_lightAngle[i] - u_lightPenumbra[i], 0.0));
                float outerC = cos(u_lightAngle[i]);
                float c = dot(-L, spotDir); // cos(angle between light dir and point)
                float denom = max(innerC - outerC, 1e-4);
                float u = clamp((innerC - c) / denom, 0.0, 1.0); // 0 at inner, 1 at outer
                float falloff = -2.0 * u * u * u + 3.0 * u * u;
                spotFactor = 1.0 - falloff;
            }
        }

        float NdotL = max(dot(n, L), 0.0);
        vec3 diffuse = baseKd * NdotL;

        // Phong specular
        vec3 R = reflect(-L, n);
        float RdotV = max(dot(R, V), 0.0);
        float specPow = (u_shininess <= 0.0) ? 0.0 : pow(RdotV, u_shininess);
        vec3 specular = u_global.z * u_ks * specPow;

        vec3 lightContribution = (diffuse + specular) * u_lightColor[i] * attenuation * spotFactor;
        color += lightContribution;
    }

    // Apply distance-based atmospheric fog
    float fogFactor = 0.0;
    if (u_fogEnable == 1) {
        float dist = length(u_camPos - v_wpos);
        float d = u_fogDensity * dist;
        // Exponential-squared fog (smooth falloff)
        // Choose density so ~98% fog at far plane using exp2 model:
        // 1 - exp(-(density*far)^2) ~= 0.98  => density ~= sqrt(-ln(0.02)) / far
        fogFactor = 1.0 - exp(-d * d);
        fogFactor = clamp(fogFactor, 0.0, 1.0);
    }
    vec3 finalColor = mix(color, u_fogColor, fogFactor);
    fragColor = vec4(finalColor, 1.0);
    outVelocity = v_velocity;
}
