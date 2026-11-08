#version 330 core
in vec2 Screen;
out vec4 FragColor;

// 1 gradient 2 image
uniform int mode;
uniform vec3 topColor;
uniform vec3 bottomColor;
uniform sampler2D image;
uniform float viewAspect;
uniform float imageAspect;

void main() {
    if (mode == 2) {
        // Fill without stretching
        vec2 uv = Screen;
        if (viewAspect > imageAspect) uv.y = (uv.y - 0.5) * imageAspect / viewAspect + 0.5;
        else uv.x = (uv.x - 0.5) * viewAspect / imageAspect + 0.5;
        FragColor = vec4(texture(image, vec2(uv.x, 1.0 - uv.y)).rgb, 1.0);
        return;
    }
    FragColor = vec4(mix(bottomColor, topColor, smoothstep(0.0, 1.0, Screen.y)), 1.0);
}
