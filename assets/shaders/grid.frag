#version 330 core
in vec3 WorldPos;
out vec4 FragColor;

uniform vec3 cameraPos;
uniform bool gridLines;
uniform bool useShadows;
uniform sampler2D shadowMap;
uniform mat4 lightSpace;

float GridLine(vec2 coord, float spacing) {
    vec2 c = coord / spacing;
    vec2 width = fwidth(c);
    vec2 g = abs(fract(c - 0.5) - 0.5) / width;
    return 1.0 - min(min(g.x, g.y), 1.0);
}

// Floor darkness
float GroundShadow() {
    vec4 p = lightSpace * vec4(WorldPos, 1.0);
    vec3 c = p.xyz / p.w * 0.5 + 0.5;
    if (c.z > 1.0 || c.x < 0.0 || c.x > 1.0 || c.y < 0.0 || c.y > 1.0) return 0.0;
    vec2 texel = 1.0 / vec2(textureSize(shadowMap, 0));
    float dark = 0.0;
    for (int x = -2; x <= 2; x++)
        for (int y = -2; y <= 2; y++)
            dark += c.z - 0.0015 > texture(shadowMap, c.xy + vec2(x, y) * texel * 1.5).r ? 1.0 : 0.0;
    return dark / 25.0;
}

void main() {
    vec2 xy = WorldPos.xy;
    float dist = length(WorldPos - cameraPos);
    float height = max(abs(cameraPos.z), 1.0);

    float minorFade = 1.0 - smoothstep(height * 6.0, height * 20.0, dist);
    float fade = 1.0 - smoothstep(height * 25.0, height * 120.0, dist);

    vec4 color = vec4(0.0);
    if (gridLines) {
        float minor = GridLine(xy, 1.0) * 0.20 * minorFade;
        float major = GridLine(xy, 10.0) * 0.42;
        color = vec4(0.60, 0.60, 0.60, max(minor, major));

        vec2 width = fwidth(xy);
        float xAxis = 1.0 - min(abs(xy.y) / (width.y * 1.5), 1.0);
        float yAxis = 1.0 - min(abs(xy.x) / (width.x * 1.5), 1.0);
        color = mix(color, vec4(0.91, 0.28, 0.33, 0.9), xAxis);
        color = mix(color, vec4(0.45, 0.78, 0.35, 0.9), yAxis);
        color.a *= fade;
    }

    if (useShadows && cameraPos.z > 0.0) {
        float shade = GroundShadow() * 0.7;
        color.rgb = mix(vec3(0.0), color.rgb, 1.0 - shade);
        color.a = max(color.a, shade);
    }

    if (color.a < 0.003) discard;
    FragColor = color;
}
