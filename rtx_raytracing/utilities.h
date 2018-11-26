#pragma once

#include <chrono>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <optional>
#include <sstream>

#define NOMINMAX
#define GLFW_EXPOSE_NATIVE_WIN32
#include "glfw3.h"
#include "glfw3native.h"

#include "glm/glm.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "glm/gtc/random.hpp"

#define VK_USE_PLATFORM_WIN32_KHR
#include "vulkan/vulkan.hpp"

// Logging macro
#ifdef _DEBUG
#define LOG_DEBUG(x) std::cout << x << "\n"
#else
#define LOG_DEBUG(x) 
#endif

static vk::PhysicalDevice physical_device;
static vk::Device device;
static vk::Queue queue;
static vk::DispatchLoaderDynamic dispatch_loader;
static vk::CommandPool command_pool;

void initialize_utilities(vk::PhysicalDevice s_physical_device, 
	vk::Device s_device, 
	vk::Queue s_queue,
	vk::DispatchLoaderDynamic s_dispatch_loader,
	vk::CommandPool s_command_pool)
{
	physical_device = s_physical_device;
	device = s_device;
	queue = s_queue;
	dispatch_loader = s_dispatch_loader;
	command_pool = s_command_pool;
}

struct alignas(8) PushConstants
{
	float resolution[2];
	float cursor[2];
	float time;
	/* Add more members here: mind the struct alignment */
};

struct WindowDetails
{
	uint32_t width;
	uint32_t height;
	std::string name;
	GLFWwindow* window;
};

struct GPUDetails
{
	std::vector<vk::QueueFamilyProperties> queues;
	vk::PhysicalDeviceFeatures features;
	vk::PhysicalDeviceProperties properties;
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
	vk::UniqueBuffer inner;
	vk::UniqueDeviceMemory device_memory;

	std::optional<vk::UniqueBufferView> view;
};

struct Image
{
	vk::UniqueImage inner;
	vk::UniqueDeviceMemory device_memory;
	
	std::optional<vk::UniqueImageView> view;
};

struct AccelerationStructure
{
	vk::UniqueHandle<vk::AccelerationStructureNVX, vk::DispatchLoaderDynamic> inner;
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

uint32_t find_memory_type(const vk::MemoryRequirements& memory_requirements, vk::MemoryPropertyFlags memory_properties)
{
	// Query available memory types
	auto physical_device_memory_properties = physical_device.getMemoryProperties();

	// Find a suitable memory type for this buffer
	for (uint32_t i = 0; i < physical_device_memory_properties.memoryTypeCount; i++)
	{
		if ((memory_requirements.memoryTypeBits & (1 << i)) &&
			(physical_device_memory_properties.memoryTypes[i].propertyFlags & memory_properties) == memory_properties)
		{
			return i;
		}
	}

	throw std::runtime_error("No suitable memory types found");
}

Buffer create_buffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags memory_properties)
{
	auto buffer_create_info = vk::BufferCreateInfo{ {}, size, usage };
	auto buffer = device.createBufferUnique(buffer_create_info);

	// Figure out memory requirements for this buffer
	auto memory_requirements = device.getBufferMemoryRequirements(buffer.get());
	uint32_t memory_type_index = find_memory_type(memory_requirements, memory_properties);

	// Allocate memory from the heap corresponding to the specified memory type index
	auto memory_allocate_info = vk::MemoryAllocateInfo{ memory_requirements.size, memory_type_index };
	auto device_memory = device.allocateMemoryUnique(memory_allocate_info);

	// Associate the newly allocated device memory with this buffer
	device.bindBufferMemory(buffer.get(), device_memory.get(), 0);

	// Finally, create a "standard" buffer view (for now)
	//auto buffer_view_create_info = vk::BufferViewCreateInfo{ {}, buffer.get(), vk::Format::eR32G32B32Sfloat, 0, size };
	//auto buffer_view = device->createBufferViewUnique(buffer_view_create_info);

	return Buffer{ std::move(buffer), std::move(device_memory), {} };
}

template<class T>
void upload(const Buffer& buffer, const std::vector<T>& data, vk::DeviceSize offset = 0)
{
	size_t upload_size = sizeof(T) * data.size();

	void* ptr = device.mapMemory(buffer.device_memory.get(), offset, VK_WHOLE_SIZE);
	memcpy(ptr, data.data(), upload_size);
	device.unmapMemory(buffer.device_memory.get());
}

AccelerationStructure build_accel(vk::AccelerationStructureTypeNVX type, vk::ArrayProxy<const vk::GeometryNVX> geometries, uint32_t instance_count)
{
	// First, create the acceleration structure
	auto accel_create_info = vk::AccelerationStructureCreateInfoNVX{}
		.setType(type)
		.setGeometryCount(geometries.size())
		.setPGeometries(geometries.data())
		.setInstanceCount(instance_count);

	auto accel = device.createAccelerationStructureNVXUnique(accel_create_info, nullptr, dispatch_loader);

	// Then, get the memory requirements for the newly created acceleration structure
	auto accel_memory_requirements_info = vk::AccelerationStructureMemoryRequirementsInfoNVX{ accel.get() };
	auto accel_memory_requirements = device.getAccelerationStructureMemoryRequirementsNVX(accel_memory_requirements_info, dispatch_loader);

	// Allocate memory for this acceleration structure, which will reside in device local memory
	uint32_t memory_type_index = find_memory_type(accel_memory_requirements.memoryRequirements, vk::MemoryPropertyFlagBits::eDeviceLocal);
	auto memory_allocate_info = vk::MemoryAllocateInfo{ accel_memory_requirements.memoryRequirements.size, memory_type_index };
	auto device_memory = device.allocateMemoryUnique(memory_allocate_info);

	// Bind the device memory to the acceleration structure
	auto bind_as_memory_info = vk::BindAccelerationStructureMemoryInfoNVX{ accel.get(), device_memory.get() };
	device.bindAccelerationStructureMemoryNVX(bind_as_memory_info, dispatch_loader);

	// Get a handle to the acceleration structure
	uint64_t handle;
	device.getAccelerationStructureHandleNVX(accel.get(), sizeof(uint64_t), &handle, dispatch_loader);

	// Get scratch memory requirements for this acceleration structure (useful later)
	auto scratch_memory_requirements = device.getAccelerationStructureScratchMemoryRequirementsNVX(accel_memory_requirements_info, dispatch_loader);

	return AccelerationStructure{ std::move(accel), std::move(device_memory), handle, scratch_memory_requirements, type };
}

void single_time_commands(std::function<void(vk::CommandBuffer)> func)
{
	auto command_buffer = std::move(device.allocateCommandBuffersUnique(vk::CommandBufferAllocateInfo{ command_pool, vk::CommandBufferLevel::ePrimary, 1 })[0]);
	
	command_buffer->begin({ vk::CommandBufferUsageFlagBits::eOneTimeSubmit });
	func(command_buffer.get());
	command_buffer->end();

	queue.submit(vk::SubmitInfo{ 0, nullptr, nullptr, 1, &command_buffer.get() }, {});
	queue.waitIdle();
}

struct GeometryDefinition
{
	std::vector<glm::vec3> vertices;
	std::vector<glm::vec4> normals;
	std::vector<uint32_t> indices;
	std::vector<glm::uvec4> primitives;

	void transform(const glm::mat3& matrix)
	{
		for (auto& v : vertices)
		{
			v = matrix * v;
		}

		for (auto& n : normals)
		{
			// TODO: n = matrix * n;
		}
	}
};

GeometryDefinition build_rect(float width = 1.0f, float height = 1.0f, const glm::vec3& center = { 0.0f, 0.0f, 0.0f })
{
	std::vector<glm::vec3> vertices =
	{
		{ -width, 0.0f, -height },
		{  width, 0.0f, -height },
		{  width, 0.0f,  height },
		{ -width, 0.0f,  height }
	};

	for (auto& v : vertices)
	{
		v += center;
	}

	std::vector<glm::vec4> normals;
	normals.resize(vertices.size(), { 0.0f, -1.0f, 0.0f, 0.0f }); // Remember -y is up

	std::vector<uint32_t> indices =
	{
		0, 1, 2, // First triangle
		0, 3, 2  // Second triangle
	};

	std::vector<glm::uvec4> primitives;
	for (size_t i = 0; i < indices.size(); i += 3)
	{
		primitives.push_back({ indices[i + 0], indices[i + 1], indices[i + 2], 0 });
	}

	return GeometryDefinition{ vertices, normals, indices, primitives };
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
		v = glm::normalize(v) * radius;
	}

	size_t vertex_index = 0;
	std::vector<glm::vec4> normals;
	normals.resize(vertices.size());
	std::generate(normals.begin(), normals.end(), [&] { return glm::vec4{ glm::normalize(vertices[vertex_index]), 0.0f }; }); // W-coordinate is not used

	for (auto &v : vertices)
	{
		v += center;
	}

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

	std::vector<glm::uvec4> primitives;
	for (size_t i = 0; i < indices.size(); i += 3)
	{
		primitives.push_back({ indices[i + 0], indices[i + 1], indices[i + 2], 0 }); // W-coordinate is not used
	}

	return GeometryDefinition{ vertices, normals, indices, primitives };
}

GeometryDefinition build_sphere(size_t u_divisions = 24, size_t v_divisions = 24, float radius = 1.0f, const glm::vec3& center = { 0.0f, 0.0f, 0.0f })
{
	std::vector<glm::vec3> vertices;
	std::vector<glm::vec4> normals;
	for (int i = 0; i <= v_divisions; ++i)
	{
		float v = i / static_cast<float>(v_divisions);		// Fraction along the v-axis, 0..1
		float phi = v * glm::pi<float>();					// Vertical angle, 0..pi

		for (int j = 0; j <= u_divisions; ++j)
		{
			float u = j / static_cast<float>(u_divisions);	// Fraction along the u-axis, 0..1
			float theta = u * (glm::pi<float>() * 2.0f);	// Rotational angle, 0..2 * pi

			// Spherical to Cartesian coordinates
			float x = cosf(theta) * sinf(phi);
			float y = cosf(phi);
			float z = sinf(theta) * sinf(phi);
			auto vertex = glm::vec3(x, y, z) * radius;

			vertices.push_back(vertex);
			normals.push_back({ glm::normalize(vertex), 0.0f }); 
		}
	}

	// Translate the sphere's vertices after creating normals
	for (auto& v : vertices)
	{
		v += center;
	}

	std::vector<uint32_t> indices;
	for (int i = 0; i < u_divisions * v_divisions + u_divisions; ++i)
	{
		indices.push_back(i);
		indices.push_back(static_cast<uint32_t>(i + u_divisions + 1));
		indices.push_back(static_cast<uint32_t>(i + u_divisions));

		indices.push_back(static_cast<uint32_t>(i + u_divisions + 1));
		indices.push_back(i);
		indices.push_back(i + 1);
	}

	std::vector<glm::uvec4> primitives;
	for (size_t i = 0; i < indices.size(); i += 3)
	{
		primitives.push_back({ indices[i + 0], indices[i + 1], indices[i + 2], 0 }); 
	}

	return GeometryDefinition{ vertices, normals, indices, primitives };
}

glm::mat4x3 get_identity_matrix()
{
	// GLM is column-major by default, so we need to construct our own
	// identity matrix here
	return glm::mat4x3{
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f
	};
}

glm::mat4x3 get_translation_matrix(const glm::vec3& translate)
{
	return glm::mat4x3{
		1.0f, 0.0f, 0.0f, translate.x,
		0.0f, 1.0f, 0.0f, translate.y,
		0.0f, 0.0f, 1.0f, translate.z
	};
}

glm::mat4x3 get_scale_matrix(const glm::vec3& scale)
{
	return glm::mat4x3{
		scale.x, 0.0f, 0.0f, 0.0f,
		0.0f, scale.y, 0.0f, 0.0f,
		0.0f, 0.0f, scale.z, 0.0f
	};
}

glm::mat4x3 get_transformation_matrix(const glm::vec3& scale, const glm::vec3& translate)
{
	return glm::mat4x3{
		scale.x, 0.0f, 0.0f, translate.x,
		0.0f, scale.y, 0.0f, translate.y,
		0.0f, 0.0f, scale.z, translate.z
	};
}