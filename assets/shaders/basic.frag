#version 330 core
in vec2 TexCoord;
in vec3 Normal;
in vec3 WorldPos;
in vec3 LocalPos;
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

uniform int noiseType;
uniform float noiseScale;
uniform float noiseDetail;
uniform float noiseContrast;
uniform float noiseBump;
uniform float noiseRough;
uniform float noiseSeed;
uniform vec3 noiseColor;
uniform vec3 objectScale;
uniform mat3 localToWorldNormal;

vec3 Hash3(vec3 p) {
    p = vec3(dot(p, vec3(127.1, 311.7, 74.7)), dot(p, vec3(269.5, 183.3, 246.1)), dot(p, vec3(113.5, 271.9, 124.6)));
    return -1.0 + 2.0 * fract(sin(p) * 43758.5453123);
}

// Smooth gradient noise
float Perlin(vec3 p) {
    vec3 i = floor(p);
    vec3 f = fract(p);
    vec3 u = f * f * f * (f * (f * 6.0 - 15.0) + 10.0);
    float n000 = dot(Hash3(i + vec3(0, 0, 0)), f - vec3(0, 0, 0));
    float n100 = dot(Hash3(i + vec3(1, 0, 0)), f - vec3(1, 0, 0));
    float n010 = dot(Hash3(i + vec3(0, 1, 0)), f - vec3(0, 1, 0));
    float n110 = dot(Hash3(i + vec3(1, 1, 0)), f - vec3(1, 1, 0));
    float n001 = dot(Hash3(i + vec3(0, 0, 1)), f - vec3(0, 0, 1));
    float n101 = dot(Hash3(i + vec3(1, 0, 1)), f - vec3(1, 0, 1));
    float n011 = dot(Hash3(i + vec3(0, 1, 1)), f - vec3(0, 1, 1));
    float n111 = dot(Hash3(i + vec3(1, 1, 1)), f - vec3(1, 1, 1));
    return mix(mix(mix(n000, n100, u.x), mix(n010, n110, u.x), u.y), mix(mix(n001, n101, u.x), mix(n011, n111, u.x), u.y), u.z) * 1.6;
}

float Fbm(vec3 p) {
    float sum = 0.0, amp = 0.5, total = 0.0;
    int octaves = int(clamp(noiseDetail, 1.0, 8.0));
    for (int k = 0; k < 8; k++) {
        if (k >= octaves) break;
        sum += Perlin(p) * amp;
        total += amp;
        p = p * 2.03 + vec3(1.7, 9.2, 3.1);
        amp *= 0.5;
    }
    return sum / total;
}

float Cells(vec3 p) {
    vec3 i = floor(p);
    vec3 f = fract(p);
    float best = 8.0;
    for (int x = -1; x <= 1; x++)
        for (int y = -1; y <= 1; y++)
            for (int z = -1; z <= 1; z++) {
                vec3 cell = vec3(x, y, z);
                vec3 point = cell + 0.5 + 0.5 * Hash3(i + cell);
                best = min(best, length(point - f));
            }
    return clamp(best, 0.0, 1.0);
}

// Pattern value from 0 to 1
float Pattern(vec3 p) {
    float t = 0.0;
    if (noiseType == 1) t = 0.5 + 0.5 * Perlin(p);
    else if (noiseType == 2) t = 0.5 + 0.5 * Fbm(p);
    else if (noiseType == 3) t = pow(1.0 - abs(sin(p.x * 1.5 + Fbm(p) * 5.0)), 5.0);
    else if (noiseType == 4) t = fract(length(p.xy) * 2.0 + Fbm(p * 0.6) * 0.8);
    else if (noiseType == 5) t = Cells(p);
    else if (noiseType == 6) t = 0.5 + 0.5 * sin((p.x + p.y * 0.3) * 6.2831 + Fbm(p) * 1.5);
    t = (t - 0.5) * (1.0 + noiseContrast * 4.0) + 0.5;
    return clamp(t, 0.0, 1.0);
}

vec3 NoisePoint() {
    return LocalPos * objectScale * noiseScale + vec3(17.31, 9.13, 3.71) * noiseSeed;
}

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

vec3 Pbr(vec3 albedo, vec3 n, vec3 v, float noiseValue) {
    float rough = clamp(roughness + noiseRough * (noiseValue - 0.5), 0.04, 1.0);
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
    float noiseValue = 0.5;
    if (noiseType > 0 && shadeMode != 2) {
        noiseValue = Pattern(NoisePoint());
        color.rgb = mix(color.rgb, noiseColor, noiseValue);
    }

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
        if (noiseType > 0 && noiseBump > 0.0 && shadeMode == 1) {
            // Bumps from pattern slope
            vec3 p = NoisePoint();
            float e = 0.02;
            vec3 slope = vec3(Pattern(p + vec3(e, 0, 0)) - noiseValue, Pattern(p + vec3(0, e, 0)) - noiseValue, Pattern(p + vec3(0, 0, e)) - noiseValue) / e;
            vec3 g = localToWorldNormal * (slope * objectScale);
            g -= n * dot(g, n);
            n = normalize(n - g * noiseBump * 0.08);
        }
        if (shadeMode == 1) {
            lit = Pbr(color.rgb, n, toEye, noiseType > 0 ? noiseValue : 0.5);
            rim = pow(1.0 - max(dot(n, toEye), 0.0), 2.0);
        } else {
            lit = Studio(color.rgb, n, toEye, rim);
        }
    }

    // Selected glow at edges
    if (highlight > 0.0) lit = mix(lit, highlightColor, clamp(highlight * (0.10 + rim * 0.6), 0.0, 1.0));
    FragColor = vec4(lit, color.a * alphaScale);
}
