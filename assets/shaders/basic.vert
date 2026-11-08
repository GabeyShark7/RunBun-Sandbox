#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;

out vec2 TexCoord;
out vec3 Normal;
out vec3 WorldPos;

uniform mat4 view;
uniform mat4 projection;
uniform mat4 model;

void main() {
    vec4 world = model * vec4(aPos, 1.0);
    gl_Position = projection * view * world;
    WorldPos = world.xyz;
    TexCoord = aTexCoord;
    Normal = mat3(transpose(inverse(model))) * aNormal;
}
