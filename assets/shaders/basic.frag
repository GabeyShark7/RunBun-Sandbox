#version 330 core
in vec2 TexCoord;
in vec3 Normal;
in vec3 WorldPos;
out vec4 FragColor;

uniform sampler2D texture1;
uniform bool hasTexture;
uniform bool conflictMode;
uniform vec4 baseColor;
uniform float highlight;
uniform vec3 highlightColor;
uniform vec3 viewPos;
uniform mat3 uvTransform;

// 0 solid 1 material 2 flat
uniform int shadeMode;
uniform float alphaScale;
uniform float roughness;
uniform float metallic;
uniform float glow;
uniform bool unlit;

uniform int lightCount;
uniform int lightType[8];
uniform vec3 lightPos[8];
uniform vec3 lightDir[8];
uniform vec3 lightColor[8];
uniform float lightPower[8];
uniform float lightCone[8];
uniform vec3 skyColor;
uniform vec3 groundColor;

uniform sampler2D shadowMap;
uniform bool useShadows;
uniform int shadowLight;
uniform mat4 lightSpace;

const float PI = 3.14159265;

float ShadowAt(vec3 world, vec3 n, vec3 l) {
    vec4 p = lightSpace * vec4(world, 1.0);
    vec3 c = p.xyz / p.w * 0.5 + 0.5;
    if (c.z > 1.0 || c.x < 0.0 || c.x > 1.0 || c.y < 0.0 || c.y > 1.0) return 1.0;
    float bias = max(0.0025 * (1.0 - dot(n, l)), 0.0006);
    vec2 texel = 1.0 / vec2(textureSize(shadowMap, 0));
    float lit = 0.0;
    for (int x = -1; x <= 1; x++)
        for (int y = -1; y <= 1; y++)
            lit += c.z - bias > texture(shadowMap, c.xy + vec2(x, y) * texel * 1.5).r ? 0.0 : 1.0;
    return lit / 9.0;
}

vec3 Studio(vec3 color, vec3 n, vec3 toEye, out float rim) {
    vec3 keyDir = normalize(vec3(0.5, -0.7, 1.0));
    vec3 fillDir = normalize(vec3(-0.6, 0.5, -0.2));
    float sky = 0.5 + 0.5 * n.z;
    float ambient = mix(0.30, 0.42, sky);
    float key = 0.50 * max(dot(n, keyDir), 0.0);
    float fill = 0.15 * max(dot(n, fillDir), 0.0);
    float head = 0.22 * max(dot(n, toEye), 0.0);
    rim = pow(1.0 - max(dot(n, toEye), 0.0), 2.0);
    return color * (ambient + key + fill + head);
}

vec3 Pbr(vec3 albedo, vec3 n, vec3 v) {
    float rough = clamp(roughness, 0.04, 1.0);
    float a = rough * rough;
    float a2 = a * a;
    vec3 f0 = mix(vec3(0.04), albedo, metallic);
    float nv = max(dot(n, v), 0.001);
    vec3 total = vec3(0.0);
    for (int i = 0; i < lightCount; i++) {
        vec3 l;
        vec3 radiance = lightColor[i] * lightPower[i];
        if (lightType[i] == 0) {
            l = -lightDir[i];
            radiance *= 3.0;
        } else {
            vec3 d = lightPos[i] - WorldPos;
            float dist2 = max(dot(d, d), 0.0001);
            l = d / sqrt(dist2);
            radiance *= 1.0 / dist2;
            if (lightType[i] == 2) {
                float cosAngle = dot(-l, lightDir[i]);
                radiance *= smoothstep(lightCone[i], mix(lightCone[i], 1.0, 0.25), cosAngle);
            }
        }
        float nl = max(dot(n, l), 0.0);
        if (nl <= 0.0) continue;
        if (useShadows && i == shadowLight) radiance *= ShadowAt(WorldPos, n, l);
        vec3 h = normalize(v + l);
        float nh = max(dot(n, h), 0.0);
        float denom = nh * nh * (a2 - 1.0) + 1.0;
        float D = a2 / (PI * denom * denom);
        float k = (rough + 1.0) * (rough + 1.0) / 8.0;
        float G = (nv / (nv * (1.0 - k) + k)) * (nl / (nl * (1.0 - k) + k));
        vec3 F = f0 + (1.0 - f0) * pow(1.0 - max(dot(h, v), 0.0), 5.0);
        vec3 spec = D * G * F / (4.0 * nv * nl + 0.001);
        vec3 kd = (1.0 - F) * (1.0 - metallic);
        total += (kd * albedo / PI + spec) * radiance * nl;
    }
    vec3 hemi = mix(groundColor, skyColor, 0.5 + 0.5 * n.z);
    vec3 ambient = hemi * albedo * (1.0 - metallic) + hemi * f0 * mix(1.7, 0.45, rough);
    vec3 color = total + ambient + albedo * glow * 1.5;
    // Soft roll off at top
    return color / (1.0 + max(max(color.r, color.g), color.b) * 0.18);
}

void main() {
    if (conflictMode) {
        FragColor = vec4(1.0, 0.0, 1.0, 1.0);
        return;
    }

    vec2 uv = (uvTransform * vec3(TexCoord, 1.0)).xy;
    vec4 color = hasTexture ? texture(texture1, uv) * vec4(1.0, 1.0, 1.0, baseColor.a) : baseColor;

    if (shadeMode == 2) {
        FragColor = vec4(color.rgb, alphaScale);
        return;
    }
    if (unlit) {
        FragColor = vec4(min(color.rgb * 1.4 + 0.15, vec3(1.0)), alphaScale);
        return;
    }

    vec3 lit = color.rgb;
    float rim = 0.0;
    if (length(Normal) > 0.001) {
        vec3 n = normalize(Normal);
        vec3 toEye = normalize(viewPos - WorldPos);
        // Light both sides
        if (dot(n, toEye) < 0.0) n = -n;
        if (shadeMode == 1) {
            lit = Pbr(color.rgb, n, toEye);
            rim = pow(1.0 - max(dot(n, toEye), 0.0), 2.0);
        } else {
            lit = Studio(color.rgb, n, toEye, rim);
        }
    }

    // Selected glow at edges
    if (highlight > 0.0) lit = mix(lit, highlightColor, clamp(highlight * (0.10 + rim * 0.6), 0.0, 1.0));
    FragColor = vec4(lit, color.a * alphaScale);
}
