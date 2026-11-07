#version 330 core
layout (location = 0) in vec3 aPos;

out vec3 WorldPos;

uniform mat4 view;
uniform mat4 projection;
uniform vec3 cameraPos;
uniform float extent;

void main() {
    WorldPos = vec3(aPos.x * extent + cameraPos.x, 0.0, aPos.z * extent + cameraPos.z);
    gl_Position = projection * view * vec4(WorldPos, 1.0);
}
