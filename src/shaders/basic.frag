#version 330 core
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D texture1;
uniform bool hasTexture;
uniform bool conflictMode;

void main() {
    if (conflictMode) {
        FragColor = vec4(1.0, 0.0, 1.0, 1.0);
    } else if (hasTexture) {
        FragColor = texture(texture1, TexCoord);
    } else {
        FragColor = vec4(1.0, 0.5, 0.2, 1.0);
    }
}