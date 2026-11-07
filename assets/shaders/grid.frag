#version 330 core
in vec3 WorldPos;
out vec4 FragColor;

uniform vec3 cameraPos;

float GridLine(vec2 coord, float spacing) {
    vec2 c = coord / spacing;
    vec2 width = fwidth(c);
    vec2 g = abs(fract(c - 0.5) - 0.5) / width;
    return 1.0 - min(min(g.x, g.y), 1.0);
}

void main() {
    vec2 xz = WorldPos.xz;
    float dist = length(WorldPos - cameraPos);
    float height = max(abs(cameraPos.y), 1.0);

    float minorFade = 1.0 - smoothstep(height * 6.0, height * 20.0, dist);
    float fade = 1.0 - smoothstep(height * 25.0, height * 120.0, dist);

    float minor = GridLine(xz, 1.0) * 0.20 * minorFade;
    float major = GridLine(xz, 10.0) * 0.42;
    vec4 color = vec4(0.62, 0.62, 0.70, max(minor, major));

    vec2 width = fwidth(xz);
    float xAxis = 1.0 - min(abs(xz.y) / (width.y * 1.5), 1.0);
    float zAxis = 1.0 - min(abs(xz.x) / (width.x * 1.5), 1.0);
    color = mix(color, vec4(0.91, 0.28, 0.33, 0.9), xAxis);
    color = mix(color, vec4(0.27, 0.55, 0.94, 0.9), zAxis);

    color.a *= fade;
    if (color.a < 0.003) discard;
    FragColor = color;
}
