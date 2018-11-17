#pragma once

#include <chrono>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <sstream>

#define NOMINMAX
#define GLFW_EXPOSE_NATIVE_WIN32
#include "glfw3.h"
#include "glfw3native.h"

#include "glm.hpp"

#define VK_USE_PLATFORM_WIN32_KHR
#include "vulkan/vulkan.hpp"

// Logging macro
#ifdef _DEBUG
#define LOG_DEBUG(x) std::cout << x << "\n"
#else
#define LOG_DEBUG(x) 
#endif

struct alignas(8) PushConstants
{
	float time;
	float __padding;
	float resolution[2];
	/* Add more members here: mind the struct alignment */
};

struct WindowDetails
{
	uint32_t width;
	uint32_t height;
	std::string name;
	GLFWwindow* window;
};

struct SurfaceDetails
{
	vk::SurfaceCapabilitiesKHR capabilities;
	std::vector<vk::SurfaceFormatKHR> formats;
	std::vector<vk::PresentModeKHR> present_modes;
};

struct SwapchainDetails
{
	vk::Format image_format;
	vk::Extent2D extent;
};

struct Buffer
{
	vk::UniqueBuffer buffer;
	vk::UniqueDeviceMemory device_memory;
};

struct Image
{
	vk::UniqueImage image;
	vk::UniqueDeviceMemory device_memory;
	vk::UniqueImageView image_view;
};

struct AccelerationStructure
{
	vk::UniqueHandle<vk::AccelerationStructureNVX, vk::DispatchLoaderDynamic> accel;
	vk::UniqueDeviceMemory device_memory;
	uint64_t handle;
	vk::MemoryRequirements2KHR scratch_memory_requirements;
	vk::AccelerationStructureTypeNVX type;
};

struct VkGeometryInstance
{
	float transform[12];
	uint32_t instanceId : 24;
	uint32_t mask : 8;
	uint32_t instanceOffset : 24;
	uint32_t flags : 8;
	uint64_t accelerationStructureHandle;
};

float get_elapsed_time()
{
	static std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
	std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();

	auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count();

	return static_cast<float>(ms) / 1000.0f;
}

VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT object_type, uint64_t object, size_t location, int32_t code, const char* layer_prefix, const char* msg, void* user_data)
{
	std::ostringstream message;

	if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT)
	{
		message << "ERROR: ";
	}
	else if (flags & VK_DEBUG_REPORT_WARNING_BIT_EXT)
	{
		message << "WARNING: ";
	}
	else if (flags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT)
	{
		message << "PERFORMANCE WARNING: ";
	}
	else if (flags & VK_DEBUG_REPORT_INFORMATION_BIT_EXT)
	{
		message << "INFO: ";
	}
	else if (flags & VK_DEBUG_REPORT_DEBUG_BIT_EXT)
	{
		message << "DEBUG: ";
	}
	message << "[" << layer_prefix << "] Code " << code << " : " << msg;
	std::cout << message.str() << std::endl;

	return VK_FALSE;
}

vk::UniqueShaderModule load_spv_into_module(const vk::UniqueDevice& device, const std::string& filename)
{
	std::ifstream file(filename, std::ios::ate | std::ios::binary);

	if (!file.is_open())
	{
		throw std::runtime_error("Failed to open file");
	}

	size_t file_size = static_cast<size_t>(file.tellg());
	std::vector<char> buffer(file_size);
	file.seekg(0);
	file.read(buffer.data(), file_size);
	file.close();

	auto shader_module_create_info = vk::ShaderModuleCreateInfo{ {}, static_cast<uint32_t>(buffer.size()), reinterpret_cast<const uint32_t*>(buffer.data()) };

	return device->createShaderModuleUnique(shader_module_create_info);
}

void image_barrier(vk::CommandBuffer command_buffer,
				   vk::Image image,
				   const vk::ImageSubresourceRange& subresource,
				   vk::AccessFlags src_access_mask,
				   vk::AccessFlags dst_access_mask,
				   vk::ImageLayout old_layout,
				   vk::ImageLayout new_layout)
{
	vk::ImageMemoryBarrier image_memory_barrier;
	image_memory_barrier.srcAccessMask = src_access_mask;
	image_memory_barrier.dstAccessMask = dst_access_mask;
	image_memory_barrier.oldLayout = old_layout;
	image_memory_barrier.newLayout = new_layout;
	image_memory_barrier.image = image;
	image_memory_barrier.subresourceRange = subresource;

	command_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands, {}, {}, {}, image_memory_barrier);
}

vk::ImageSubresourceRange get_single_layer_resource(vk::ImageAspectFlags image_aspect_flags = vk::ImageAspectFlagBits::eColor)
{
	return vk::ImageSubresourceRange{ image_aspect_flags, 0, 1, 0, 1 };
}

struct GeometryDefinition
{
	std::vector<glm::vec3> vertices;
	std::vector<glm::vec3> normals;
	std::vector<uint32_t> indices;
};

GeometryDefinition build_rect(float width = 1.0f, float height = 1.0f, const glm::vec3& center = { 0.0f, 0.0f, 0.0f })
{
	std::vector<glm::vec3> vertices =
	{
		{ -width, -height, 0.0f },
		{  width, -height, 0.0f },
		{  width,  height, 0.0f },
		{ -width,  height, 0.0f }
	};

	for (auto& v : vertices)
	{
		v += center;
	}

	std::vector<glm::vec3> normals;
	normals.resize(vertices.size(), { 0.0f, 0.0f, 1.0f });

	std::vector<uint32_t> indices =
	{
		0, 1, 2, // First triangle
		0, 3, 2  // Second triangle
	};

	return GeometryDefinition{ vertices, normals, indices };
}

GeometryDefinition build_icosphere(float radius = 1.0f, const glm::vec3& center = { 0.0f, 0.0f, 0.0f })
{
	// See: http://blog.andreaskahler.com/2009/06/creating-icosphere-mesh-in-code.html
	const float t = (1.0f + sqrtf(5.0f)) / 2.0f;

	std::vector<glm::vec3> vertices =
	{
		{ -1.0f,  t,     0.0f },
		{  1.0f,  t,     0.0f },
		{ -1.0f, -t,     0.0f },
		{  1.0f, -t,     0.0f },
		{  0.0f, -1.0f,  t },
		{  0.0f,  1.0f,  t },
		{  0.0f, -1.0f, -t },
		{  0.0f,  1.0f, -t },
		{  t,     0.0f, -1.0f },
		{  t,     0.0f,  1.0f },
		{ -t,     0.0f, -1.0f },
		{ -t,     0.0f,  1.0f }
	};

	for (auto &v : vertices)
	{
		v = glm::normalize(v) * radius + center;
	}

	size_t vertex_index = 0;
	std::vector<glm::vec3> normals;
	normals.resize(vertices.size());

	std::generate(normals.begin(), normals.end(), [&] { return glm::normalize(vertices[vertex_index]); });

	std::vector<uint32_t> indices =
	{
		0,  11, 5,
		0,  5,  1,
		0,  1,  7,
		0,  7,  10,
		0,  10, 11,
		1,  5,  9,
		5,  11, 4,
		11, 10, 2,
		10, 7,  6,
		7,  1,  8,
		3,  9,  4,
		3,  4,  2,
		3,  2,  6,
		3,  6,  8,
		3,  8,  9,
		4,  9,  5,
		2,  4,  11,
		6,  2,  10,
		8,  6,  7,
		9,  8,  1
	};

	return GeometryDefinition{ vertices, normals, indices };
}

