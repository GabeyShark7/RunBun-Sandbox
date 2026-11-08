#version 330 core
flat in int vKind;
out vec4 FragColor;

uniform vec4 colors[4];
uniform bool roundPoints;
uniform float alphaScale;

void main() {
    if (roundPoints) {
        vec2 d = gl_PointCoord - vec2(0.5);
        float r = dot(d, d);
        if (r > 0.25) discard;
        if (r > 0.12 && vKind == 0) {
            FragColor = vec4(0.06, 0.06, 0.08, alphaScale);
            return;
        }
    }
    vec4 color = colors[clamp(vKind, 0, 3)];
    FragColor = vec4(color.rgb, color.a * alphaScale);
}
