#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in float aFlag;

uniform mat4 mvp;
uniform float bias;
uniform float pointSize;

flat out int vKind;

void main() {
    gl_Position = mvp * vec4(aPos, 1.0);
    gl_Position.z -= bias * gl_Position.w;
    vKind = int(aFlag + 0.5);
    gl_PointSize = vKind == 1 ? pointSize + 2.0 : pointSize;
}
