#include "Shader.h"
#include <glad/glad.h>
#include <fstream>
#include <sstream>
#include <iostream>

//Read the entire file into a string
static std::string ReadFile(const char* path) {
    std::ifstream file(path);
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

static unsigned int CompileShader(unsigned int type, const std::string& source) {
    //Creates a shader object of the given type
    unsigned int shader = glCreateShader(type);
    const char* src = source.c_str();
    //Attches the source code
    glShaderSource(shader, 1, &src, nullptr);
    //Compiles the source code
    glCompileShader(shader);

    //Checks if compile was a success
    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    //If not then give the error message
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        std::cout << "Shader compile error:\n" << infoLog << std::endl;
    }
    return shader;
}

unsigned int LoadShader(const char* vertexPath, const char* fragmentPath) {
    std::string vertexCode = ReadFile(vertexPath);
    std::string fragmentCode = ReadFile(fragmentPath);

    unsigned int vertexShader = CompileShader(GL_VERTEX_SHADER, vertexCode);
    unsigned int fragmentShader = CompileShader(GL_FRAGMENT_SHADER, fragmentCode);

    //Create a program to link shaders and actually link
    unsigned int program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);

    //Checks if the linking was a success
    int success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    //If not then give the error message
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(program, 512, nullptr, infoLog);
        std::cout << "Shader link error:\n" << infoLog << std::endl;
    }

    // shaders are now linked
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return program;
}