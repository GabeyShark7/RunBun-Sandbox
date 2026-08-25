#include "Framebuffer.h"
#include <iostream>

void Framebuffer::Init(int w, int h) {
    width = w;
    height = h;

    glGenFramebuffers(1, &fbo);          // create the framebuffer object
    glBindFramebuffer(GL_FRAMEBUFFER, fbo); // select it as the active render target

    // --- Color texture (what we'll actually see) ---
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    // creates an empty texture of the given size, RGB color, no data yet — we'll render INTO it

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); // smooth scaling when shrinking
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR); // smooth scaling when enlarging

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
    // attach this texture to the framebuffer as its color output

    // --- Depth buffer (needed so 3D objects render in correct front/back order) ---
    glGenRenderbuffers(1, &rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cout << "Framebuffer is not complete!\n"; // sanity check — catches setup mistakes

    glBindFramebuffer(GL_FRAMEBUFFER, 0); // unbind, back to rendering to the actual screen for now
}

void Framebuffer::Bind() {
    glBindFramebuffer(GL_FRAMEBUFFER, fbo); // redirect drawing into our off-screen texture
    glViewport(0, 0, width, height);        // match the OpenGL viewport to our framebuffer's size
}

void Framebuffer::Unbind() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0); // redirect drawing back to the real screen
}

unsigned int Framebuffer::GetTexture() {
    return texture; // ImGui will use this ID to display the rendered image
}

void Framebuffer::Resize(int w, int h) {
    if (w == width && h == height) return; // no change, skip rebuilding
    width = w;
    height = h;

    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);

    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
}