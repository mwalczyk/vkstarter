# vkstarter
Simple starter code for building applications with Vulkan and C++.

## Description
This is meant to be a sort of "template" for creating Vulkan applications. It uses `vulkan.hpp`, which is included in LunarG's Vulkan SDK.
Note that there are many checks and features that are omitted in this project that would otherwise be present in a "real" application. This
is meant to be a starting point!

## Tested On
Windows 8.1, NVIDIA GeForce GTX 970M, v`1.1.70.1` Vulkan SDK.

## To Build
1. Clone this repo.
2. Inside the repo, create two new folders named `build` and `third_party`.
3. Download the [Vulkan SDK for Windows](https://vulkan.lunarg.com/sdk/home#windows). Make sure the `VK_SDK_PATH` environment
   variable is defined.
4. Download the [GLFW pre-compiled binaries](http://www.glfw.org/download.html) (64-bit Windows) and place inside the `third_party` directory. Rename this folder to `glfw`.
5. Optionally, run `vkstarter/compile.bat` to convert the included `GLSL` shaders to `SPIR-V`. This will be run automatically as a pre-build 
event by Visual Studio.
6. Open the Visual Studio 2015 solution file.
7. Build the included project.

### License

:copyright: The Interaction Department 2018

[Creative Commons Attribution 4.0 International License](https://creativecommons.org/licenses/by/4.0/)

