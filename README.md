## RunBun Sandbox

A live 3D model viewer and editor built from scratch in C++ with OpenGL. Import FBX and glTF models, move around the scene with a first person camera, and edit object transforms and textures in real time through a custom ImGui based editor UI.

## Features
- Custom OpenGL rendering pipeline with GLSL shaders
- First person camera with WASD movement and mouse look
- Model importing via Assimp (FBX, glTF, GLB)
- Texture loading and per object texture management
- Multi object scene support with selection and deletion
- Editor style panels: Scene, Objects, Properties, and Console

## Planned
- Multiplayer support so multiple people can view and edit the same scene together
- Voice and text chat between connected users

## Built with
C++, OpenGL, GLFW, GLAD, GLM, Dear ImGui, Assimp, stb_image
