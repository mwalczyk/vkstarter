#!/bin/bash

$VULKAN_SDK/bin/glslangValidator -V -o vkstarter/shaders/vert.spv vkstarter/shaders/shader.vert
$VULKAN_SDK/bin/glslangValidator -V -o vkstarter/shaders/frag.spv vkstarter/shaders/shader.frag