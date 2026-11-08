#version 330 core
out vec2 Screen;

void main() {
    vec2 corners[3] = vec2[3](vec2(-1.0, -1.0), vec2(3.0, -1.0), vec2(-1.0, 3.0));
    Screen = corners[gl_VertexID] * 0.5 + 0.5;
    gl_Position = vec4(corners[gl_VertexID], 0.999, 1.0);
}
