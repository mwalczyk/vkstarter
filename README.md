# rtx_raytracing
🔨 Simple starter code for building raytracing applications with Vulkan, C++, and NVIDIA RTX graphics cards.

## Description
This is meant to be a sort of "template" for creating raytracing applications with Vulkan. It uses `vulkan.hpp`, which is included in LunarG's Vulkan SDK. Note that there are many checks and features that are omitted in this project that would otherwise be present in a "real" application. 

By default, `rtx_raytracing` sets up a simple scene with a handful of spheres and a ground plane, as seen in the screenshot above. Window resizing (and subsequent swapchain re-creation) is enabled by default.

## Tested On
Windows 10 Pro x64, NVIDIA Quadro RTX 6000, `1.1.85.0` Vulkan SDK with Visual Studio 2017.

## To Build
1. Clone this repo.
2. Inside the repo, create a new folder named `third_party`.
3. Download the [Vulkan SDK for Windows](https://vulkan.lunarg.com/sdk/home#sdk/downloadConfirm/1.1.85.0/windows/VulkanSDK-1.1.85.0-Installer.exe). Make sure the `VK_SDK_PATH` environment
   variable is defined on your system.
4. Download the [GLFW pre-compiled binaries](http://www.glfw.org/download.html) (64-bit Windows) and place inside the `third_party` directory. Rename this folder to `glfw`.
5. Download [GLM](https://github.com/g-truc/glm/tags) and place inside the `third_party` directory. Rename this folder to `glm`.
6. Optionally, run `rtx_raytracing/shaders/compile.bat` to convert the included `GLSL` shaders to `SPIR-V`. This will be run automatically as a pre-build event by Visual Studio.
7. Open the Visual Studio 2017 solution file.
8. Build the included project.

### License

:copyright: Obscura Digital 2018
