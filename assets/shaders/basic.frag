#version 330 core
in vec2 TexCoord;
in vec3 Normal;
out vec4 FragColor;

uniform sampler2D texture1;
uniform bool hasTexture;
uniform bool conflictMode;

void main() {
    if (conflictMode) {
        FragColor = vec4(1.0, 0.0, 1.0, 1.0);
        return;
    }

    vec4 baseColor = hasTexture ? texture(texture1, TexCoord) : vec4(1.0, 0.5, 0.2, 1.0);

    float light = 1.0;
    if (length(Normal) > 0.001) {
        vec3 n = normalize(Normal);
        vec3 keyDir = normalize(vec3(0.5, 1.0, 0.7));
        vec3 fillDir = normalize(vec3(-0.6, -0.2, -0.5));
        light = 0.35 + 0.65 * max(dot(n, keyDir), 0.0) + 0.15 * max(dot(n, fillDir), 0.0);
    }

    FragColor = vec4(baseColor.rgb * light, baseColor.a);
}
