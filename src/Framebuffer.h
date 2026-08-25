#pragma once
#include <glad/glad.h>

class Framebuffer {
public:
    void Init(int width, int height); // creates the framebuffer + texture at a given size
    void Bind();                       // redirect rendering into this framebuffer
    void Unbind();                     // redirect rendering back to the actual screen
    unsigned int GetTexture();         // returns the texture ID so ImGui can display it
    void Resize(int width, int height); // rebuilds the framebuffer if the panel size changes

private:
    unsigned int fbo;      // framebuffer object ID
    unsigned int texture;  // color texture attached to the framebuffer
    unsigned int rbo;      // renderbuffer for depth (needed for correct 3D rendering later)
    int width, height;
};