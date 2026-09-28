## RunBun Sandbox

A live 3D model viewer and editor built from scratch in C++ with OpenGL. Import FBX and glTF models, move around the scene with a first person camera, and edit object transforms and textures in real time through a custom ImGui based editor UI.

## Clarity (PLEASE READ)
- I did NOT make an early repo for this project. I wouldn't say it was very in depth before my first commit, but there was some decent progress done. I wasn't thinking at the time of making this a public project, but did manage to capture some early images of the UI and features. See the images below.

## Features
- Custom OpenGL rendering pipeline with GLSL shaders
- First person camera with WASD movement and mouse look
- Model importing via Assimp (FBX, glTF, GLB)
- Texture loading and per object texture management
- Multi object scene support with selection and deletion
- Editor style panels: Scene, Objects, Properties, and Console

## Planned
- Multiplayer support so multiple people can view and edit the same scene together
- **09/28/26 - I plan to add the networking soon. I've been very busy but will get that done. I'm thinking Rust with rust-libp2p, but might change my mind in the next couple days.**
- Voice and text chat between connected users

## Built with
C++, OpenGL, GLFW, GLAD, GLM, Dear ImGui, Assimp, stb_image

## Very Early Build/UI
<img width="652" height="364" alt="IMG_8113" src="https://github.com/user-attachments/assets/1e162b6b-ebc5-4c48-a05c-7ab017d3f408" />

## Mid Build w/Decent looking UI
<img width="2335" height="1197" alt="IMG_8111" src="https://github.com/user-attachments/assets/e2fb001d-aadf-45dd-ad50-e1112736d135" />

## Starting to add the ability to customize and use templates for UI (Current)
<img width="4032" height="3024" alt="IMG_8086" src="https://github.com/user-attachments/assets/3dbbd0f1-a08c-4efb-834f-c8faf73e8a09" />



