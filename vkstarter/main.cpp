#include <iostream>
#include <sstream>
#include <fstream>
#include <limits>
#include <chrono>

#define NOMINMAX
#define GLFW_EXPOSE_NATIVE_WIN32
#include "glfw3.h"
#include "glfw3native.h"

#define VK_USE_PLATFORM_WIN32_KHR
#include "vulkan/vulkan.hpp"

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

float get_elapsed_time()
{
	static std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
	std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();

	auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count();

	return static_cast<float>(ms) / 1000.0f;
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

class Application
{
public:
	Application(uint32_t width, uint32_t height, const std::string& name) :
		width{ width }, height{ height }, name{ name }
	{
		setup();
	}

	~Application()
	{	
		// Wait for all work on the GPU to finish
		device->waitIdle();

		// Clean up GLFW objects
		glfwDestroyWindow(window);
		glfwTerminate();
	}

	static void on_window_resized(GLFWwindow* window, int width, int height) 
	{
		Application* app = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
		app->resize();
	}

	void resize()
	{
		device->waitIdle();

		int new_width;
		int new_height;
		glfwGetWindowSize(window, &new_width, &new_height);
		width = new_width;
		height = new_height;
		LOG_DEBUG("Window resized to " + std::to_string(width) + " x " + std::to_string(height));

		swapchain.reset();
		render_pass.reset();
		pipeline.reset();

		// We do not need to explicitly clear the framebuffers or swapchain image views, since that is taken
		// care of by the `initialize_*()` methods below

		initialize_swapchain();
		initialize_render_pass();
		initialize_pipeline();
		initialize_framebuffers();
	}

	void setup()
	{
		initialize_window();
		initialize_instance();
		initialize_device();
		initialize_surface();
		initialize_swapchain();
		initialize_render_pass();


		initialize_rtx_geometry();
		initialize_rtx_pipeline();
		

		initialize_pipeline();
		initialize_framebuffers();
		initialize_command_pool();
		initialize_command_buffers();
		initialize_synchronization_primitives();
	}

	void initialize_window()
	{
		glfwInit();
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

		window = glfwCreateWindow(width, height, name.c_str(), nullptr, nullptr);

		glfwSetWindowUserPointer(window, this);
		glfwSetWindowSizeCallback(window, on_window_resized);
	}

	void initialize_instance()
	{
		std::vector<const char*> layers;
		std::vector<const char*> extensions{ VK_EXT_DEBUG_REPORT_EXTENSION_NAME, VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_WIN32_SURFACE_EXTENSION_NAME };
#ifdef _DEBUG
		layers.push_back("VK_LAYER_LUNARG_standard_validation");
#endif
		auto application_info = vk::ApplicationInfo{ name.c_str(), VK_MAKE_VERSION(1, 0, 0), name.c_str(), VK_MAKE_VERSION(1, 0, 0), VK_API_VERSION_1_1 };

		instance = vk::createInstanceUnique(vk::InstanceCreateInfo{ {}, &application_info, static_cast<uint32_t>(layers.size()), layers.data(), static_cast<uint32_t>(extensions.size()), extensions.data() });

		// vulkan.hpp provides a per-function dispatch mechanism by accepting a dispatch class as last parameter in each function call -
		// this is required to use extensions
		dispatch_loader = vk::DispatchLoaderDynamic{ instance.get() };
#ifdef _DEBUG
		auto debug_report_callback_create_info = vk::DebugReportCallbackCreateInfoEXT{ vk::DebugReportFlagBitsEXT::eError | vk::DebugReportFlagBitsEXT::eWarning, debug_callback };

		debug_report_callback = instance->createDebugReportCallbackEXT(debug_report_callback_create_info, nullptr, dispatch_loader);
		LOG_DEBUG("Initializing debug report callback");
#endif

	}

	void initialize_device()
	{
		// First, we select a physical device
		auto physical_devices = instance->enumeratePhysicalDevices();
		assert(!physical_devices.empty());
		physical_device = physical_devices[0];

		auto queue_family_properties = physical_device.getQueueFamilyProperties();

		const float priority = 0.0f;
		auto predicate = [](const vk::QueueFamilyProperties& item) { return item.queueFlags & vk::QueueFlagBits::eGraphics; };
		auto queue_create_info = vk::DeviceQueueCreateInfo{}
			.setPQueuePriorities(&priority)
			.setQueueCount(1)
			.setQueueFamilyIndex(static_cast<uint32_t>(std::distance(queue_family_properties.begin(), std::find_if(queue_family_properties.begin(), queue_family_properties.end(), predicate))));
		LOG_DEBUG("Using queue family at index [ " << queue_create_info.queueFamilyIndex << " ], which supports graphics operations");

		// Save the index of the chosen queue family
		queue_family_index = queue_create_info.queueFamilyIndex;

		// Then, we construct a logical device around the chosen physical device
		const std::vector<const char*> device_extensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_NVX_RAYTRACING_EXTENSION_NAME };
		auto device_create_info = vk::DeviceCreateInfo{}
			.setPQueueCreateInfos(&queue_create_info)
			.setQueueCreateInfoCount(1)
			.setPpEnabledExtensionNames(device_extensions.data())
			.setEnabledExtensionCount(static_cast<uint32_t>(device_extensions.size()));

		device = physical_device.createDeviceUnique(device_create_info);

		const uint32_t queue_index = 0;
		queue = device->getQueue(queue_family_index, queue_index);
	}

	void initialize_surface()
	{	
		auto surface_create_info = vk::Win32SurfaceCreateInfoKHR{ {}, GetModuleHandle(nullptr), glfwGetWin32Window(window) };

		surface = instance->createWin32SurfaceKHRUnique(surface_create_info);
	}

	void initialize_swapchain()
	{
		surface_capabilities = physical_device.getSurfaceCapabilitiesKHR(surface.get());
		surface_formats = physical_device.getSurfaceFormatsKHR(surface.get());
		surface_present_modes = physical_device.getSurfacePresentModesKHR(surface.get());
		auto surface_support = physical_device.getSurfaceSupportKHR(queue_family_index, surface.get());

		swapchain_image_format = vk::Format::eB8G8R8A8Unorm;
		swapchain_extent = vk::Extent2D{ width, height };

		auto swapchain_create_info = vk::SwapchainCreateInfoKHR{}
			.setPresentMode(vk::PresentModeKHR::eMailbox)
			.setImageExtent(swapchain_extent)
			.setImageFormat(swapchain_image_format)
			.setImageArrayLayers(1)
			.setImageUsage(vk::ImageUsageFlagBits::eColorAttachment)
			.setMinImageCount(surface_capabilities.minImageCount + 1)
			.setPreTransform(surface_capabilities.currentTransform)
			.setClipped(true)
			.setSurface(surface.get());

		swapchain = device->createSwapchainKHRUnique(swapchain_create_info);

		// Retrieve the images from the swapchain
		swapchain_images = device->getSwapchainImagesKHR(swapchain.get());
		LOG_DEBUG("There are [ " << swapchain_images.size() << " ] images in the swapchain");

		// Create an image view for each image in the swapchain
		swapchain_image_views.clear();

		const auto subresource_range = vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };
		for (const auto& image : swapchain_images)
		{
			auto image_view_create_info = vk::ImageViewCreateInfo{ {}, image, vk::ImageViewType::e2D, swapchain_image_format, {}, subresource_range };
			swapchain_image_views.push_back(device->createImageViewUnique(image_view_create_info));
		}
		LOG_DEBUG("Created [ " << swapchain_image_views.size() << " ] image views");
	}

	void initialize_render_pass()
	{
		auto attachment_description = vk::AttachmentDescription{}
			.setFormat(swapchain_image_format)
			.setLoadOp(vk::AttachmentLoadOp::eClear)
			.setStoreOp(vk::AttachmentStoreOp::eStore)
			.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
			.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
			.setFinalLayout(vk::ImageLayout::ePresentSrcKHR);

		const uint32_t attachment_index = 0;
		auto attachment_reference = vk::AttachmentReference{ attachment_index, vk::ImageLayout::eColorAttachmentOptimal };

		auto subpass_description = vk::SubpassDescription{}
			.setPColorAttachments(&attachment_reference)
			.setColorAttachmentCount(1);

		auto subpass_dependency = vk::SubpassDependency{}
			.setSrcSubpass(VK_SUBPASS_EXTERNAL)
			.setDstSubpass(0)
			.setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
			.setSrcAccessMask({})
			.setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
			.setDstAccessMask(vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite);

		auto render_pass_create_info = vk::RenderPassCreateInfo{ {}, 1, &attachment_description, 1, &subpass_description, 1, &subpass_dependency };

		render_pass = device->createRenderPassUnique(render_pass_create_info);
	}

	struct Buffer
	{
		vk::UniqueBuffer buffer;
		vk::UniqueDeviceMemory device_memory;
	};

	uint32_t find_memory_type(uint32_t types, vk::MemoryPropertyFlags memory_properties)
	{
		// Query available memory types
		auto physical_device_memory_properties = physical_device.getMemoryProperties();

		// Find a suitable memory type for this buffer
		for (uint32_t i = 0; i < physical_device_memory_properties.memoryTypeCount; i++)
		{
			if ((types & (1 << i)) &&
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
		auto buffer = device->createBufferUnique(buffer_create_info);

		// Figure out memory requirements for this buffer
		auto memory_requirements = device->getBufferMemoryRequirements(buffer.get());
		uint32_t memory_type_index = find_memory_type(memory_requirements.memoryTypeBits, memory_properties);

		// Allocate memory from the heap corresponding to the specified memory type index
		auto memory_allocate_info = vk::MemoryAllocateInfo{ memory_requirements.size, memory_type_index };
		auto device_memory = device->allocateMemoryUnique(memory_allocate_info);

		// Associate the newly allocated device memory with this buffer
		device->bindBufferMemory(buffer.get(), device_memory.get(), 0);

		return Buffer{ std::move(buffer), std::move(device_memory) };
	}
	
	template<class T>
	void upload(const Buffer& buffer, const std::vector<T>& data)
	{
		size_t upload_size = sizeof(T) * data.size();

		void* ptr = device->mapMemory(buffer.device_memory.get(), 0, upload_size);
		memcpy(ptr, data.data(), upload_size);
		device->unmapMemory(buffer.device_memory.get());
	}

	// RTX
	void initialize_rtx_geometry()
	{
		const std::vector<float> vertices =
		{
			0.25f, 0.25f, 0.0f,
			0.75f, 0.25f, 0.0f,
			0.50f, 0.75f, 0.0f
		};

		const std::vector<uint32_t> indices = { 0, 1, 2 };

		// Describe how the memory associated with these buffers will be accessed
		auto memory_properties = vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible;

		// Describe the intended usage of these buffers
		auto vertex_buffer_usage = vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eRaytracingNVX;
		auto index_buffer_usage = vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eRaytracingNVX;
		
		// Create the buffers (and device memory)
		vertex_buffer = create_buffer(sizeof(float) * vertices.size(), vertex_buffer_usage, memory_properties);
		index_buffer = create_buffer(sizeof(uint32_t) * indices.size(), index_buffer_usage, memory_properties);
		
		LOG_DEBUG("Created vertex and index buffers");

		upload(vertex_buffer, vertices);
		upload(index_buffer, indices);

		LOG_DEBUG("Uploaded vertex and index data to buffers");

		auto geometry_triangles = vk::GeometryTrianglesNVX{}
			.setIndexCount(3)
			.setIndexData(index_buffer.buffer.get())
			.setIndexType(vk::IndexType::eUint32)
			.setVertexCount(3)
			.setVertexData(vertex_buffer.buffer.get())
			.setVertexFormat(vk::Format::eR32G32B32Sfloat)
			.setVertexStride(sizeof(float) * 3);

		auto geometry_data = vk::GeometryDataNVX{ geometry_triangles };

		// By default, the geometry type is set to triangles
		auto geometry = vk::GeometryNVX{}
			.setFlags(vk::GeometryFlagBitsNVX::eOpaque)
			.setGeometry(geometry_data);
	}

	// RTX
	void initialize_rtx_acceleration_structures()
	{

	}

	// RTX
	void initialize_rtx_pipeline()
	{
		{
			auto physical_device_ray_tracing_properties = vk::PhysicalDeviceRaytracingPropertiesNVX{};
			auto physical_device_properties_2 = vk::PhysicalDeviceProperties2{};
			
			// Attach a pointer to the ray tracing struct extension
			physical_device_properties_2.pNext = &physical_device_ray_tracing_properties;

			// Finally, populate the structs 
			physical_device.getProperties2(&physical_device_properties_2);

			LOG_DEBUG("Physical device ray tracing properties:");
			LOG_DEBUG("		Max geometry count: " << physical_device_ray_tracing_properties.maxGeometryCount);
			LOG_DEBUG("		Max recursion depth: " << physical_device_ray_tracing_properties.maxRecursionDepth);
			LOG_DEBUG("		Shader header size: " << physical_device_ray_tracing_properties.shaderHeaderSize);
		}

		// Load the 3 shader modules that will be used to build the RTX pipeline
		const std::string path_prefix = "";
		const std::string rgen_spv_path = path_prefix + "rgen.spv";
		const std::string chit_spv_path = path_prefix + "rchit.spv";
		const std::string miss_spv_path = path_prefix + "rmiss.spv";
		auto rgen_module = load_spv_into_module(device, rgen_spv_path);
		auto chit_module = load_spv_into_module(device, chit_spv_path);
		auto miss_module = load_spv_into_module(device, miss_spv_path);
		LOG_DEBUG("Successfully loaded RTX shader modules");

		// Gather shader stage create infos
		const char* entry_point = "main";
		auto rgen_stage_create_info = vk::PipelineShaderStageCreateInfo{ {}, vk::ShaderStageFlagBits::eRaygenNVX, rgen_module.get(), entry_point };
		auto chit_stage_create_info = vk::PipelineShaderStageCreateInfo{ {}, vk::ShaderStageFlagBits::eClosestHitNVX, chit_module.get(), entry_point };
		auto miss_stage_create_info = vk::PipelineShaderStageCreateInfo{ {}, vk::ShaderStageFlagBits::eMissNVX, miss_module.get(), entry_point };
		const vk::PipelineShaderStageCreateInfo shader_stage_create_infos[] = { rgen_stage_create_info, chit_stage_create_info, miss_stage_create_info };

		// Create a descriptor set layout that accommodates 2 descriptors:
		// 1. Acceleration structure
		// 2. Storage image
		auto layout_binding_0 = vk::DescriptorSetLayoutBinding{ 0, vk::DescriptorType::eAccelerationStructureNVX, 1, vk::ShaderStageFlagBits::eRaygenNVX };
		auto layout_binding_1 = vk::DescriptorSetLayoutBinding{ 1, vk::DescriptorType::eStorageImage, 1, vk::ShaderStageFlagBits::eRaygenNVX };
		const vk::DescriptorSetLayoutBinding descriptor_set_layout_bindings[] = { layout_binding_0, layout_binding_1 };

		auto descriptor_set_layout_create_info = vk::DescriptorSetLayoutCreateInfo{}
			.setBindingCount(2)
			.setPBindings(descriptor_set_layout_bindings);

		descriptor_set_layout = device->createDescriptorSetLayoutUnique(descriptor_set_layout_create_info);

		// Create a pipeline layout for the RTX pipeline
		auto pipeline_layout_create_info = vk::PipelineLayoutCreateInfo{}
			.setSetLayoutCount(1)
			.setPSetLayouts(&descriptor_set_layout.get());

		raytracing_pipeline_layout = device->createPipelineLayoutUnique(pipeline_layout_create_info);

		// Group 0: raygen
		// Group 1: closest hit
		// Group 2: miss
		const uint32_t group_numbers[] = { 0, 1, 2 };

		// Build a ray tracing pipeline object
		auto raytracing_pipeline_create_info = vk::RaytracingPipelineCreateInfoNVX{}
			.setStageCount(3)
			.setPStages(shader_stage_create_infos)
			.setPGroupNumbers(group_numbers)
			.setLayout(raytracing_pipeline_layout.get())
			.setMaxRecursionDepth(1);

		raytracing_pipeline = device->createRaytracingPipelineNVXUnique({}, raytracing_pipeline_create_info, nullptr, dispatch_loader);
		LOG_DEBUG("Successfully create RTX pipeline");
	}

	// RTX 
	void initialize_rtx_shader_binding_table()
	{

	}

	void initialize_pipeline()
	{
		// First, load the shader modules
		const std::string path_prefix = "";
		const std::string vs_spv_path = path_prefix + "vert.spv";
		const std::string fs_spv_path = path_prefix + "frag.spv";
		auto vs_module = load_spv_into_module(device, vs_spv_path);
		auto fs_module = load_spv_into_module(device, fs_spv_path);
		LOG_DEBUG("Successfully loaded shader modules");
		
		// Then, create a pipeline layout
		auto push_constant_range = vk::PushConstantRange{ vk::ShaderStageFlagBits::eFragment, 0, sizeof(float) * 4 };
		auto pipeline_layout_create_info = vk::PipelineLayoutCreateInfo{}
			.setPPushConstantRanges(&push_constant_range)
			.setPushConstantRangeCount(1);

		pipeline_layout = device->createPipelineLayoutUnique(pipeline_layout_create_info /* Add additional push constants or descriptor sets here */ );

		// Finally, create the pipeline
		const char* entry_point = "main";
		auto vs_stage_create_info = vk::PipelineShaderStageCreateInfo{ {}, vk::ShaderStageFlagBits::eVertex, vs_module.get(), entry_point };
		auto fs_stage_create_info = vk::PipelineShaderStageCreateInfo{ {}, vk::ShaderStageFlagBits::eFragment, fs_module.get(), entry_point };
		const vk::PipelineShaderStageCreateInfo shader_stage_create_infos[] = { vs_stage_create_info, fs_stage_create_info };

		auto vertex_input_state_create_info = vk::PipelineVertexInputStateCreateInfo{};

		auto input_assembly_create_info = vk::PipelineInputAssemblyStateCreateInfo{ {}, vk::PrimitiveTopology::eTriangleList };

		const float min_depth = 0.0f;
		const float max_depth = 1.0f;
		const vk::Viewport viewport{ 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), min_depth, max_depth };
		const vk::Rect2D scissor{ { 0, 0 }, swapchain_extent };
		auto viewport_state_create_info = vk::PipelineViewportStateCreateInfo{ {}, 1, &viewport, 1, &scissor };

		auto rasterization_state_create_info = vk::PipelineRasterizationStateCreateInfo{}
			.setFrontFace(vk::FrontFace::eClockwise)
			.setCullMode(vk::CullModeFlagBits::eBack)
			.setLineWidth(1.0f);

		auto multisample_state_create_info = vk::PipelineMultisampleStateCreateInfo{};
		
		auto color_blend_attachment_state = vk::PipelineColorBlendAttachmentState{}
			.setColorWriteMask(vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);

		auto color_blend_state_create_info = vk::PipelineColorBlendStateCreateInfo{}
			.setPAttachments(&color_blend_attachment_state)
			.setAttachmentCount(1);

		auto graphics_pipeline_create_info = vk::GraphicsPipelineCreateInfo{ 
			{}, 
			2, 
			shader_stage_create_infos, 
			&vertex_input_state_create_info,
			&input_assembly_create_info, 
			nullptr, /* Add tessellation state here */
			&viewport_state_create_info, 
			&rasterization_state_create_info,
			&multisample_state_create_info, 
			nullptr, /* Add depth stencil state here */
			&color_blend_state_create_info, 
			nullptr, /* Add dynamic state here */ 
			pipeline_layout.get(), 
			render_pass.get() 
		};

		pipeline = device->createGraphicsPipelineUnique({}, graphics_pipeline_create_info);
		LOG_DEBUG("Created graphics pipeline");
	}

	void initialize_framebuffers()
	{
		framebuffers.clear();

		const uint32_t framebuffer_layers = 1;
		for (const auto& image_view : swapchain_image_views)
		{
			auto framebuffer_create_info = vk::FramebufferCreateInfo{ {}, render_pass.get(), 1, &image_view.get(), width, height, framebuffer_layers };
			framebuffers.push_back(device->createFramebufferUnique(framebuffer_create_info));
		}
		LOG_DEBUG("Created [ " << framebuffers.size() << " ] framebuffers");
	}

	void initialize_command_pool()
	{
		command_pool = device->createCommandPoolUnique(vk::CommandPoolCreateInfo{ vk::CommandPoolCreateFlagBits::eResetCommandBuffer, queue_family_index });
	}

	void initialize_command_buffers()
	{
		command_buffers = device->allocateCommandBuffersUnique(vk::CommandBufferAllocateInfo{ command_pool.get(), vk::CommandBufferLevel::ePrimary, static_cast<uint32_t>(framebuffers.size()) });
		LOG_DEBUG("Allocated [ " << command_buffers.size() << " ] command buffers");
	}

	void record_command_buffer(uint32_t index)
	{
		const vk::ClearValue clear = std::array<float, 4>{ 0.0f, 0.0f, 0.0f, 1.0f };
		const vk::Rect2D render_area{ { 0, 0 }, swapchain_extent };

		PushConstants push_constants = 		
		{ 
			get_elapsed_time(), 
			0.0f,  /* Padding */
			static_cast<float>(width), 
			static_cast<float>(height) 
		};

		command_buffers[index]->begin(vk::CommandBufferBeginInfo{ vk::CommandBufferUsageFlagBits::eSimultaneousUse });
		command_buffers[index]->beginRenderPass(vk::RenderPassBeginInfo{ render_pass.get(), framebuffers[index].get(), render_area, 1, &clear }, vk::SubpassContents::eInline);
		command_buffers[index]->bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline.get());
		command_buffers[index]->pushConstants(pipeline_layout.get(), vk::ShaderStageFlagBits::eFragment, 0, sizeof(push_constants), &push_constants);
		command_buffers[index]->draw(6, 1, 0, 0);
		command_buffers[index]->endRenderPass();
		command_buffers[index]->end();
	}

	void initialize_synchronization_primitives()
	{
		semaphore_image_available = device->createSemaphoreUnique({});
		sempahore_render_finished = device->createSemaphoreUnique({});

		for (size_t i = 0; i < command_buffers.size(); ++i)
		{			
			// Create each fence in a signaled state, so that the first call to `waitForFences` in the draw loop doesn't throw any errors
			fences.push_back(device->createFenceUnique({ vk::FenceCreateFlagBits::eSignaled }));
		}
	}
	
	void draw()
	{
		while (!glfwWindowShouldClose(window)) 
		{
			glfwPollEvents();

			// Submit a command buffer after acquiring the index of the next available swapchain image
			auto index = device->acquireNextImageKHR(swapchain.get(), (std::numeric_limits<uint64_t>::max)(), semaphore_image_available.get(), {}).value;

			// If the command buffer we want to (re)use is still pending on the GPU, wait for it then reset its fence
			device->waitForFences(fences[index].get(), true, (std::numeric_limits<uint64_t>::max)());
			device->resetFences(fences[index].get());

			// Now, we know that we can safely (re)use this command buffer
			record_command_buffer(index);
			
			const vk::PipelineStageFlags wait_stages[] = { vk::PipelineStageFlagBits::eColorAttachmentOutput };
			auto submit_info = vk::SubmitInfo{ 1, &semaphore_image_available.get(), wait_stages, 1, &command_buffers[index].get(), 1, &sempahore_render_finished.get() };
			queue.submit(submit_info, fences[index].get());

			// Present the final rendered image to the swapchain
			auto present_info = vk::PresentInfoKHR{ 1, &sempahore_render_finished.get(), 1, &swapchain.get(), &index };
			queue.presentKHR(present_info);
		}
	}

private:
	uint32_t width;
	uint32_t height;
	std::string name;

	GLFWwindow* window;

	vk::SurfaceCapabilitiesKHR surface_capabilities;
	std::vector<vk::SurfaceFormatKHR> surface_formats;
	std::vector<vk::PresentModeKHR> surface_present_modes;

	vk::Format swapchain_image_format;
	vk::Extent2D swapchain_extent;

	vk::PhysicalDevice physical_device;
	vk::DebugReportCallbackEXT debug_report_callback;
	vk::Queue queue;
	uint32_t queue_family_index;

	vk::UniqueInstance instance;
	vk::DispatchLoaderDynamic dispatch_loader;
	vk::UniqueDevice device;
	vk::UniqueSurfaceKHR surface;
	vk::UniqueSwapchainKHR swapchain;
	vk::UniqueRenderPass render_pass;
	vk::UniquePipelineLayout pipeline_layout;
	vk::UniquePipeline pipeline;

	Buffer vertex_buffer;
	Buffer index_buffer;
	vk::UniqueDescriptorSetLayout descriptor_set_layout;
	vk::UniquePipelineLayout raytracing_pipeline_layout;
	vk::UniqueHandle<vk::Pipeline, vk::DispatchLoaderDynamic> raytracing_pipeline;

	vk::UniqueCommandPool command_pool;
	vk::UniqueSemaphore semaphore_image_available;
	vk::UniqueSemaphore sempahore_render_finished;
	std::vector<vk::Image> swapchain_images;
	std::vector<vk::UniqueImageView> swapchain_image_views;
	std::vector<vk::UniqueFramebuffer> framebuffers;
	std::vector<vk::UniqueCommandBuffer> command_buffers;
	std::vector<vk::UniqueFence> fences;
};

int main()
{
	Application app{ 800, 600, "vkstarter" };
	app.draw();

	return EXIT_SUCCESS;
}