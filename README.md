# vkstarter
Simple starter code for building applications with Vulkan and C++.

## Description
This is meant to be a sort of "template" for creating Vulkan applications. It uses `vulkan.hpp`, which is included in LunarG's Vulkan SDK.
Note that there are many checks and features that are omitted in this project that would otherwise be present in a "real" application. This
is meant to be a starting point!

## Tested On
Windows 8.1 with version `1.1.70.1` of LunarG's Vulkan SDK.

## To Build
1. Clone this repo.
2. Inside the repo, create two new folders named `build` and `third_party`.
3. Download the Vulkan SDK for Windows from LunarG's website.
4. Download the GLFW pre-compiled binaries and place inside the `third_party` directory. Rename this folder to `glfw`.
5. Run `vkstarter/compile.bat` to convert the included `GLSL` shaders to `SPIR-V`.
6. Open the Visual Studio 2015 solution file.
7. Build the included project.

### License

:copyright: The Interaction Department 2018

[Creative Commons Attribution 4.0 International License](https://creativecommons.org/licenses/by/4.0/)

