#version 330 core
in vec3 v_n;
in vec3 v_wpos;
in vec2 v_uv;
in vec2 v_velocity;
in vec3 v_objPos;

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

// planet params
uniform bool  u_isPlanet;       // per-draw flag
uniform float u_time;           // global scene time
uniform vec3  u_planetColorA;   // dark band color
uniform vec3  u_planetColorB;   // light band color
// terrain / sand params
uniform bool  u_isSand;

layout(location=0) out vec4 fragColor;
layout(location=1) out vec2 outVelocity;
layout(location=2) out vec4 outNormal;

const float PI      = 3.14159265358979;
const float EPSILON = 0.01;
const float PHI     = (1.0 + sqrt(5.0)) / 2.0;

// Simple 4D hash -> [0,1]
float hash41(vec4 p) {
    float h = dot(p, vec4(127.1, 311.7, 74.7, 157.3));
    return fract(sin(h) * 43758.5453123);
}

// Value noise -> [-1,1]
float noise(vec3 p, int s) {
    vec3 f = floor(p);
    vec3 q = fract(p);

    // smooth interpolation
    q *= q * (3.0 - 2.0 * q);
    q *= q * (3.0 - 2.0 * q);

    float n000 = hash41(vec4(f + vec3(0,0,0), float(s)));
    float n001 = hash41(vec4(f + vec3(0,0,1), float(s)));
    float n010 = hash41(vec4(f + vec3(0,1,0), float(s)));
    float n011 = hash41(vec4(f + vec3(0,1,1), float(s)));
    float n100 = hash41(vec4(f + vec3(1,0,0), float(s)));
    float n101 = hash41(vec4(f + vec3(1,0,1), float(s)));
    float n110 = hash41(vec4(f + vec3(1,1,0), float(s)));
    float n111 = hash41(vec4(f + vec3(1,1,1), float(s)));

    float nz  = mix(mix(n000, n001, q.z),
                    mix(n010, n011, q.z), q.y);
    float nz2 = mix(mix(n100, n101, q.z),
                    mix(n110, n111, q.z), q.y);
    float n = mix(nz, nz2, q.x);

    return n * 2.0 - 1.0;
}

float sandNoise2D(vec2 p) {
    // just reuse 3D noise with z=0 and a different seed
    return noise(vec3(p, 0.0), 1) * 0.5 + 0.5; // -> [0,1]
}

vec3 tange(vec3 p) {
    return normalize(cross(p, vec3(0, 1, 0)));
}

vec3 cotch(vec3 p) {
    return normalize(cross(p, tange(p)));
}

// Shadertoy-ish cval() adapted to use u_time & our noise()
float cval(vec3 p) {
    p /= length(p);
    float t = 0.0;
    float q = 1.0;
    p *= vec3(1.7, 5.3, 1.7);

    for (int n = 0; n < 10; n++) {
        float angle = u_time * 0.05 / sqrt(abs(q) + 1e-5);
        float c = cos(angle);
        float s = sin(angle);
        mat2 rot = mat2(c, 0.5, -0.5, c);
        p.xz *= rot;

        float v = noise(p * q, 0) / q;

        vec2 h = (v - vec2(
            noise(p * q - 2.0 * tange(p) * EPSILON, 0),
            noise(p * q -      cotch(p) * EPSILON, 0)
        ) / q) / EPSILON;

        t += tanh(v * 7.5);

        h /= length(h) + 0.5;
        mat2 twist = mat2(0.2 * v, 0.5 * v,
                         -0.5 * v, 0.2 * v);
        h *= twist;

        p += h.x * tange(p) + h.y * cotch(p);
        q *= -1.35;
    }
    return pow(abs(t * 0.12 + 0.8), 1.5);
}

vec3 planetNormal(vec3 objPos) {
    vec3 p = normalize(objPos);

    float center = cval(p);
    float dx = cval(p - vec3(EPSILON, 0, 0));
    float dy = cval(p - vec3(0, EPSILON, 0));
    float dz = cval(p - vec3(0, 0, EPSILON));

    vec3 grad = (center - vec3(dx, dy, dz)) * (-0.5);
    return normalize(grad + p); // combine with geometric normal
}

vec3 planetBaseColor(vec3 objPos) {
    float val = cval(normalize(objPos));
    vec3 colDark  = pow(u_planetColorA, vec3(2.0));
    vec3 colLight = pow(u_planetColorB, vec3(2.0));
    vec3 baseCol = mix(colDark, colLight, val);
    return sqrt(max(baseCol, vec3(0.0)));
}

// Smooth scalar pattern for dunes, reusing cval()
float dunePattern(vec3 objPos) {
    // Map the terrain grid into some "fake sphere" coords
    vec3 p = vec3(objPos.x * 1.7,
                  objPos.z * 1.2,
                  objPos.y * 1.4);

    // Make it flow over time (wind)
    p.xy += vec2(u_time * 0.03, u_time * 0.015);

    // cval already normalizes p internally
    float v = cval(p);     // ~0..1-ish
    return clamp(v, 0.0, 1.0);
}

void main() {
    vec3 n = normalize(v_n);
    vec3 V = normalize(u_camPos - v_wpos);

    // Base diffuse color
    float b = clamp(u_blend, 0.0, 1.0);

    vec3 baseKd;
    vec3 planetKd = vec3(0.0);

    if (u_isPlanet) {
          // ignore texture; full procedural color
          planetKd = planetBaseColor(v_objPos);
          baseKd   = planetKd;  // kd = planet colors
          // also tweak n to bumpy planet normal
          n = planetNormal(v_objPos);
      } else {
          if (u_hasTexture == 1) {
              vec3 texColor = texture(u_tex, v_uv * u_texRepeat).rgb;
              baseKd = u_global.y * u_kd * (1.0 - b) + texColor * b;
          } else {
              baseKd = u_global.y * u_kd;
          }
          // --- extra stylization for dunes ---
          if (u_isSand) {
            // 1) Base sand palette
              vec3 colDark = baseKd * vec3(0.85, 0.65, 0.55);
              vec3 colLight = baseKd * vec3(1.05, 0.95, 0.85);

              // 2) Smooth scalar field over the dunes
              float pat = dunePattern(v_objPos);          // 0..1
              pat = smoothstep(0.2, 0.8, pat);           // tighten contrast a bit

              // Smooth “bands” driven by that pattern
              baseKd = mix(colDark, colLight, pat);

              // 3) Soft sand-blowing highlights riding on the same pattern

              // Fake wind direction in world XZ
              vec2 windDir = normalize(vec2(1.0, 0.3));
              float alongWind = dot(v_wpos.xz, windDir);

              // A gentle travelling wave
              float wave = alongWind * 6.0 + u_time * 0.8;
              float s = 0.5 + 0.5 * sin(wave);           // 0..1

              // Make thin soft streaks (no harsh steps)
              float streak = pow(s, 6.0);                // sharp bright ridges

              // Modulate streaks by the underlying pattern so they “flow” with it
              float streakMask = streak * (0.4 + 0.6 * pat);

              vec3 blowCol = colLight * 1.1;            // slightly brighter sand
              float blowStrength = 0.28;                // how visible the blowing is

              baseKd = mix(baseKd, blowCol, streakMask * blowStrength);

              // World/object "up" axis: terrain height is along Z
              float hWorld = v_objPos.z;

              // How many contour lines per unit height
              float contourFreq = 60.0;

              // Repeating [0,1) phase along height
              float phase = hWorld * contourFreq;
              float fracH = fract(phase);          // 0..1 repeating each band

              // Distance to the line center (0.5)
              float distToLine = abs(fracH - 0.5); // 0 at line center, 0.5 mid-gap

              // Make a thin ring around distToLine ~ 0
              float lineWidth = 0.01;              // visual thickness of contour
              float contour = 1.0 - smoothstep(lineWidth,
                                               lineWidth * 2.0,
                                               distToLine);
              // contour ≈ 1 only in a narrow band, 0 elsewhere

              // Only draw on slopes so big flats don’t get busy
              vec3 up = vec3(0.0, 0.0, 1.0);       // normalized up (matches height axis)
              float slopeAmt = 1.0 - abs(dot(n, up));   // 0 flat, 1 steep
              float slopeMask = smoothstep(0.1, 0.8, slopeAmt);

              float contourMask = contour * slopeMask;

              // Dark-ish ink lines (don’t go full black so lighting still reads)
              vec3  contourColor    = vec3(0.03, 0.01, 0.0);
              float contourStrength = 1.0;            // 0–1 strength

              baseKd = mix(baseKd, contourColor, contourMask * contourStrength);
            }
      }

    // Ambient
    vec3 color;
    if (u_isPlanet) {
        // --- Fake hemisphere + rim lighting for planets ---
        // planetKd is the procedural color from planetBaseColor()

        // Hemisphere factor based on world-space "up" (n.y)
        //  - top of sphere brighter, bottom darker
        float hemi = n.y * 0.5 + 0.5;          // [-1,1] -> [0,1]
        float lightFactor = mix(0.15, 1.0, hemi); // never darker than 15%

        vec3 hemiLit = planetKd * lightFactor;

        // Optional soft rim to avoid super-dead edges
        float rim = pow(1.0 - max(dot(n, V), 0.0), 2.0);
        vec3 rimColor = planetKd * 0.25;      // subtle, same hue family

        color = hemiLit + rim * rimColor;
        } else {

            color = u_global.x * u_ka;

            // Lighting loop
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
    outNormal = vec4(normalize(n) * 0.5 + 0.5, 1.0);

}
