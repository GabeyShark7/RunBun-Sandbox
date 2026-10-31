#include "Triangle.h"
#include <glad/glad.h>

void Triangle::Init(unsigned int shader) {
    float vertices[] = {
        -0.5f, -0.5f, 0.0f, 
         0.5f, -0.5f, 0.0f, 
         0.0f,  0.5f, 0.0f
    };

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    shaderProgram = shader;
}

void Triangle::Draw(glm::mat4 view, glm::mat4 projection) {
    glUseProgram(shaderProgram);

    //Find the uniform locations in the shader by name, then upload the matrices
    int viewLoc = glGetUniformLocation(shaderProgram, "view");
    int projLoc = glGetUniformLocation(shaderProgram, "projection");
    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, &view[0][0]);
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, &projection[0][0]);

    //The shader is shared with models, so reset everything else it reads
    glm::mat4 model = glm::mat4(1.0f);
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, &model[0][0]);
    glUniform1i(glGetUniformLocation(shaderProgram, "hasTexture"), 0);
    glUniform1i(glGetUniformLocation(shaderProgram, "conflictMode"), 0);

    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}