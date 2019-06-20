# vkstarter
🔨 Simple starter code for building applications with Vulkan and C++.

<p>
  <img src="https://github.com/mwalczyk/vkstarter/blob/master/screenshots/screenshot.png" alt="screenshot" width="300" height="auto"/>
</p>

## Description
This is meant to be a sort of "template" for creating Vulkan applications. It uses `vulkan.hpp`, which is included in LunarG's Vulkan SDK. Note that there are many checks and features that are omitted in this project that would otherwise be present in a "real" application. 

By default, `vkstarter` simply sets up a full-screen quad, as seen in the screenshot above. For simplicity sake, vertex positions are hard-coded in the vertex shader. Push constants are used to communicate application time and window resolution to the fragment shader stage. Window resizing (and subsequent swapchain re-creation) is also enabled by default.

## Tested On
- Ubuntu 18.04
- NVIDIA GeForce GTX 1070
- Vulkan SDK `1.1.106.0`

## To Build
1. Clone this repo and initialize submodules (GLFW): 
```shell
git submodule init
git submodule update
```
3. Download the [Vulkan SDK](https://vulkan.lunarg.com/sdk/home) for your OS. Make sure the `VULKAN_SDK` environment variable is defined on your system.
4. Compile the included shader files using `glslangValidator`:
```shell
sh ./compile_shaders.sh
```
4. Finally, from the root directory, run the following commands:
```shell
mkdir build
cd build
cmake ..
make

./VkStarter
```

### License
[Creative Commons Attribution 4.0 International License](https://creativecommons.org/licenses/by/4.0/)

