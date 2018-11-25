#include "scene.h"

#define RAYTRACING

static const GeometryDefinition geometry_def = build_icosphere(0.5f, { 0.0f, 0.0f, 3.0f });

class Application
{
public:

	Application(uint32_t width, uint32_t height, const std::string& name) :
		window_details{ width, height, name, nullptr }
	{
		setup();
	}

	~Application()
	{	
		// Wait for all work on the GPU to finish
		device->waitIdle();

		// Clean up GLFW objects
		glfwDestroyWindow(window_details.window);
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
		glfwGetWindowSize(window_details.window, &new_width, &new_height);

		// Store new window dimensions
		window_details.width = new_width;
		window_details.height = new_height;
		LOG_DEBUG("Window resized to " + std::to_string(window_details.width) + " x " + std::to_string(window_details.height));

		swapchain.reset();
		render_pass.reset();
		pipeline.reset();

#if defined(RAYTRACING)
		offscreen_image.inner.reset();
		offscreen_image.view.reset();
		offscreen_image.device_memory.reset();
		shader_binding_table_buffer.inner.reset();
		shader_binding_table_buffer.device_memory.reset();
#endif

		// We do not need to explicitly clear the framebuffers or swapchain image views, since that is taken
		// care of by the `initialize_*()` methods below

		initialize_swapchain();
		initialize_render_pass();
		initialize_pipeline();
		initialize_framebuffers();
#if defined(RAYTRACING)
		initialize_offscreen_image();
		initialize_shader_binding_table();
#endif
		update_descriptor_sets();
	}

	void setup()
	{
		initialize_window();
		initialize_instance();
		initialize_device();
		initialize_surface();
		initialize_swapchain();
		initialize_render_pass();
		initialize_command_pool();
		initialize_descriptor_set_layout();
		initialize_pipeline();
		initialize_framebuffers();
		initialize_command_buffers();
		initialize_synchronization_primitives();
#if defined(RAYTRACING)
		// The scene must be created after command pool allocation, since it submits a series
		// of command buffers to build the acceleration structures
		initialize_offscreen_image();
		initialize_shader_binding_table();
		
#endif
		initialize_scene();
		initialize_descriptor_set();

		// Write descriptor set
		update_descriptor_sets();
	}

	void initialize_window()
	{
		glfwInit();
		
		// Do not create an OpenGL context
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

		window_details.window = glfwCreateWindow(window_details.width, window_details.height, window_details.name.c_str(), nullptr, nullptr);

		glfwSetWindowUserPointer(window_details.window, this);
		glfwSetWindowSizeCallback(window_details.window, on_window_resized);
	}

	void initialize_instance()
	{
		std::vector<const char*> layers;
		std::vector<const char*> extensions{ VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_WIN32_SURFACE_EXTENSION_NAME };
#ifdef _DEBUG
		// Only enable the standard validation layers if we are compiling for debug
		layers.push_back("VK_LAYER_LUNARG_standard_validation");
		extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
#endif
		auto application_info = vk::ApplicationInfo{ window_details.name.c_str(), VK_MAKE_VERSION(1, 0, 0), window_details.name.c_str(), VK_MAKE_VERSION(1, 0, 0), VK_API_VERSION_1_1 };

		instance = vk::createInstanceUnique(vk::InstanceCreateInfo{ {}, &application_info, static_cast<uint32_t>(layers.size()), layers.data(), static_cast<uint32_t>(extensions.size()), extensions.data() });

		// `vulkan.hpp` provides a per-function dispatch mechanism by accepting a dispatch class as last parameter in each function call -
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
		// First, we select a physical device (here, we simply select the first device available)
		auto physical_devices = instance->enumeratePhysicalDevices();
		assert(!physical_devices.empty());
		physical_device = physical_devices[0];

		auto queue_family_properties = physical_device.getQueueFamilyProperties();

		// Find a queue that supports graphics operations
		const float priority = 0.0f;
		auto predicate = [](const vk::QueueFamilyProperties& item) { return item.queueFlags & vk::QueueFlagBits::eGraphics; };
		auto queue_create_info = vk::DeviceQueueCreateInfo{}
			.setPQueuePriorities(&priority)
			.setQueueCount(1)
			.setQueueFamilyIndex(static_cast<uint32_t>(std::distance(queue_family_properties.begin(), std::find_if(queue_family_properties.begin(), queue_family_properties.end(), predicate))));
		LOG_DEBUG("Using queue family at index [ " << queue_create_info.queueFamilyIndex << " ], which supports graphics operations");

		// Save the index of the chosen queue family
		queue_family_index = queue_create_info.queueFamilyIndex;

		// Enable any "special" device features that we might need - `vertexPipelineStoresAndAtomics`
		// is required for the ray generation shader
		vk::PhysicalDeviceFeatures physical_device_features;
		physical_device_features.vertexPipelineStoresAndAtomics = true;
		// Examples...
		//physical_device_features.geometryShader = true;
		//physical_device_features.imageCubeArray = true;

		// Then, we construct a logical device around the chosen physical device
		const std::vector<const char*> device_extensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_NVX_RAYTRACING_EXTENSION_NAME };
		auto device_create_info = vk::DeviceCreateInfo{}
			.setPQueueCreateInfos(&queue_create_info)
			.setQueueCreateInfoCount(1)
			.setPpEnabledExtensionNames(device_extensions.data())
			.setEnabledExtensionCount(static_cast<uint32_t>(device_extensions.size()))
			.setPEnabledFeatures(&physical_device_features);

		device = physical_device.createDeviceUnique(device_create_info);

		const uint32_t queue_index = 0;
		queue = device->getQueue(queue_family_index, queue_index);
	}

	void initialize_surface()
	{	
		auto surface_create_info = vk::Win32SurfaceCreateInfoKHR{ {}, GetModuleHandle(nullptr), glfwGetWin32Window(window_details.window) };

		surface = instance->createWin32SurfaceKHRUnique(surface_create_info);
		LOG_DEBUG("Successfully created window surface");
	}

	void initialize_swapchain()
	{
		// Record surface details
		surface_details.capabilities = physical_device.getSurfaceCapabilitiesKHR(surface.get());
		surface_details.formats = physical_device.getSurfaceFormatsKHR(surface.get());
		surface_details.present_modes = physical_device.getSurfacePresentModesKHR(surface.get());
		auto surface_support = physical_device.getSurfaceSupportKHR(queue_family_index, surface.get());

		swapchain_details.image_format = vk::Format::eB8G8R8A8Unorm;
		swapchain_details.extent = vk::Extent2D{ window_details.width, window_details.height };

		// Note that the swapchain images need to be created with the `vk::ImageUsageFlagBits::eTransferDst`
		// flag, as they will receive image data from the offscreen storage image used for raytracing
		auto swapchain_create_info = vk::SwapchainCreateInfoKHR{}
			.setPresentMode(vk::PresentModeKHR::eMailbox)
			.setImageExtent(swapchain_details.extent)
			.setImageFormat(swapchain_details.image_format)
			.setImageArrayLayers(1)
			.setImageUsage(vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferDst)
			.setMinImageCount(surface_details.capabilities.minImageCount + 1)
			.setPreTransform(surface_details.capabilities.currentTransform)
			.setClipped(true)
			.setSurface(surface.get());

		swapchain = device->createSwapchainKHRUnique(swapchain_create_info);

		// Retrieve the images from the swapchain
		swapchain_images = device->getSwapchainImagesKHR(swapchain.get());
		LOG_DEBUG("There are [ " << swapchain_images.size() << " ] images in the swapchain");

		// Create an image view for each image in the swapchain
		swapchain_image_views.clear();

		for (const auto& image : swapchain_images)
		{
			auto image_view_create_info = vk::ImageViewCreateInfo{ {}, image, vk::ImageViewType::e2D, swapchain_details.image_format, {}, get_single_layer_resource() };
			swapchain_image_views.push_back(device->createImageViewUnique(image_view_create_info));
		}
		LOG_DEBUG("Created [ " << swapchain_image_views.size() << " ] image views");
	}

	void initialize_render_pass()
	{
		auto attachment_description = vk::AttachmentDescription{}
			.setFormat(swapchain_details.image_format)
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

	void initialize_descriptor_set_layout()
	{
		// Create a descriptor set layout that accommodates 2 descriptors: an acceleration structure and a storage image
		const std::vector<vk::DescriptorSetLayoutBinding> descriptor_set_layout_bindings = 
		{ 
			vk::DescriptorSetLayoutBinding{ 0, vk::DescriptorType::eAccelerationStructureNVX, 1, vk::ShaderStageFlagBits::eRaygenNVX }, 
			vk::DescriptorSetLayoutBinding{ 1, vk::DescriptorType::eStorageImage, 1, vk::ShaderStageFlagBits::eRaygenNVX },
			
			// There are 3 descriptors at binding #2 and #3: one for each mesh geometry (normals and primitives)
			vk::DescriptorSetLayoutBinding{ 2, vk::DescriptorType::eStorageBuffer, 3, vk::ShaderStageFlagBits::eClosestHitNVX },
			vk::DescriptorSetLayoutBinding{ 3, vk::DescriptorType::eStorageBuffer, 3, vk::ShaderStageFlagBits::eClosestHitNVX }
		};

		auto descriptor_set_layout_create_info = vk::DescriptorSetLayoutCreateInfo{ {}, static_cast<uint32_t>(descriptor_set_layout_bindings.size()), descriptor_set_layout_bindings.data() };
		descriptor_set_layout = device->createDescriptorSetLayoutUnique(descriptor_set_layout_create_info);
	}

	void initialize_pipeline()
	{
		const std::string path_prefix = "";
		const char* entry_point = "main";

		// Create a pipeline layout (this can be shared across the graphics and raytracing pipelines)
		auto push_constant_range = vk::PushConstantRange{}
			.setOffset(0)
			.setSize(sizeof(PushConstants))
#if defined(RAYTRACING)
			.setStageFlags(vk::ShaderStageFlagBits::eRaygenNVX);
#else
			.setStageFlags(vk::ShaderStageFlagBits::eFragment);
#endif
		pipeline_layout = device->createPipelineLayoutUnique(vk::PipelineLayoutCreateInfo{ {}, 1, &descriptor_set_layout.get(), 1, &push_constant_range });

#if defined(RAYTRACING)
		// Retrieve information about this system's raytracing capabilities
		{
			auto physical_device_properties_2 = vk::PhysicalDeviceProperties2{};

			// Attach a pointer to the raytracing struct extension
			physical_device_properties_2.pNext = &raytracing_properties;

			// Finally, populate the structs 
			physical_device.getProperties2(&physical_device_properties_2);
			LOG_DEBUG("Physical device ray tracing properties:");
			LOG_DEBUG("		Max geometry count: " << raytracing_properties.maxGeometryCount);
			LOG_DEBUG("		Max recursion depth: " << raytracing_properties.maxRecursionDepth);
			LOG_DEBUG("		Shader header size: " << raytracing_properties.shaderHeaderSize);
		}

		// Load the 3 shader modules that will be used to build the raytracing pipeline
		auto rgen_module = load_spv_into_module(device, path_prefix + "pri_rgen.spv");

		auto pri_chit_module = load_spv_into_module(device, path_prefix + "pri_rchit.spv");
		auto sec_chit_module = load_spv_into_module(device, path_prefix + "sec_rchit.spv");

		auto pri_miss_module = load_spv_into_module(device, path_prefix + "pri_rmiss.spv");
		auto sec_miss_module = load_spv_into_module(device, path_prefix + "sec_rmiss.spv");
		LOG_DEBUG("Successfully loaded RTX shader modules");

		// Gather shader stage create infos
		auto rgen_stage_create_info = vk::PipelineShaderStageCreateInfo{ {}, vk::ShaderStageFlagBits::eRaygenNVX, rgen_module.get(), entry_point };
		
		auto pri_chit_stage_create_info = vk::PipelineShaderStageCreateInfo{ {}, vk::ShaderStageFlagBits::eClosestHitNVX, pri_chit_module.get(), entry_point };
		auto sec_chit_stage_create_info = vk::PipelineShaderStageCreateInfo{ {}, vk::ShaderStageFlagBits::eClosestHitNVX, sec_chit_module.get(), entry_point };
		
		auto pri_miss_stage_create_info = vk::PipelineShaderStageCreateInfo{ {}, vk::ShaderStageFlagBits::eMissNVX, pri_miss_module.get(), entry_point };
		auto sec_miss_stage_create_info = vk::PipelineShaderStageCreateInfo{ {}, vk::ShaderStageFlagBits::eMissNVX, sec_miss_module.get(), entry_point };

		const std::vector<vk::PipelineShaderStageCreateInfo> shader_stage_create_infos = 
		{ 
			rgen_stage_create_info, 
			pri_chit_stage_create_info, 
			sec_chit_stage_create_info,
			pri_miss_stage_create_info,
			sec_miss_stage_create_info 
		};

		// Group 0: ray generation
		// Group 1 and 2: closest hit
		// Group 3 and 4: miss
		const uint32_t group_numbers[] = { 0, 1, 2, 3, 4 };

		// Build a ray tracing pipeline object
		auto raytracing_pipeline_create_info = vk::RaytracingPipelineCreateInfoNVX{}
			.setStageCount(static_cast<uint32_t>(shader_stage_create_infos.size()))
			.setPStages(shader_stage_create_infos.data())
			.setPGroupNumbers(group_numbers)
			.setLayout(pipeline_layout.get())
			.setMaxRecursionDepth(10);

		pipeline = device->createRaytracingPipelineNVXUnique({}, raytracing_pipeline_create_info, nullptr, dispatch_loader);
		LOG_DEBUG("Successfully created raytracing pipeline");
#else
		// Load the 2 shader modules that will be used to build the graphics pipeline
		auto vs_module = load_spv_into_module(device, path_prefix + "vert.spv");
		auto fs_module = load_spv_into_module(device, path_prefix + "frag.spv");
		LOG_DEBUG("Successfully loaded shader modules");

		// Gather shader stage create infos
		auto vs_stage_create_info = vk::PipelineShaderStageCreateInfo{ {}, vk::ShaderStageFlagBits::eVertex, vs_module.get(), entry_point };
		auto fs_stage_create_info = vk::PipelineShaderStageCreateInfo{ {}, vk::ShaderStageFlagBits::eFragment, fs_module.get(), entry_point };
		const std::vector<vk::PipelineShaderStageCreateInfo> shader_stage_create_infos = { vs_stage_create_info, fs_stage_create_info };

		auto vertex_input_binding_description = vk::VertexInputBindingDescription{ 0, sizeof(geometry_def.vertices[0]) };
		auto vertex_input_attribute_description = vk::VertexInputAttributeDescription{ 0, 0, vk::Format::eR32G32B32Sfloat };
		auto vertex_input_state_create_info = vk::PipelineVertexInputStateCreateInfo{}
			.setVertexBindingDescriptionCount(1)
			.setPVertexBindingDescriptions(&vertex_input_binding_description)
			.setVertexAttributeDescriptionCount(1)
			.setPVertexAttributeDescriptions(&vertex_input_attribute_description);

		auto input_assembly_create_info = vk::PipelineInputAssemblyStateCreateInfo{ {}, vk::PrimitiveTopology::eTriangleList };

		const float min_depth = 0.0f;
		const float max_depth = 1.0f;
		const vk::Viewport viewport{ 0.0f, 0.0f, static_cast<float>(window_details.width), static_cast<float>(window_details.height), min_depth, max_depth };
		const vk::Rect2D scissor{ { 0, 0 }, swapchain_details.extent };
		auto viewport_state_create_info = vk::PipelineViewportStateCreateInfo{ {}, 1, &viewport, 1, &scissor };

		auto rasterization_state_create_info = vk::PipelineRasterizationStateCreateInfo{}
			.setFrontFace(vk::FrontFace::eClockwise)
			.setCullMode(vk::CullModeFlagBits::eNone) // For now, turn off face culling
			.setLineWidth(1.0f);

		auto multisample_state_create_info = vk::PipelineMultisampleStateCreateInfo{};
		
		auto color_blend_attachment_state = vk::PipelineColorBlendAttachmentState{}
			.setColorWriteMask(vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);

		auto color_blend_state_create_info = vk::PipelineColorBlendStateCreateInfo{}
			.setPAttachments(&color_blend_attachment_state)
			.setAttachmentCount(1);

		// Build a graphics pipeline object
		auto graphics_pipeline_create_info = vk::GraphicsPipelineCreateInfo{ 
			{}, 
			static_cast<uint32_t>(shader_stage_create_infos.size()),
			shader_stage_create_infos.data(), 
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
		LOG_DEBUG("Successfully created graphics pipeline");
#endif
	}

	void initialize_framebuffers()
	{
		framebuffers.clear();

		auto framebuffer_create_info = vk::FramebufferCreateInfo{ {}, render_pass.get(), 1, {}, window_details.width, window_details.height, 1 };
		for (const auto& image_view : swapchain_image_views)
		{
			framebuffer_create_info.setPAttachments(&image_view.get());
			framebuffers.push_back(device->createFramebufferUnique(framebuffer_create_info));
		}
		LOG_DEBUG("Created [ " << framebuffers.size() << " ] framebuffers");
	}

	void initialize_command_pool()
	{
		command_pool = device->createCommandPoolUnique(vk::CommandPoolCreateInfo{ vk::CommandPoolCreateFlagBits::eResetCommandBuffer, queue_family_index });
		LOG_DEBUG("Successfully created command pool");

		// Save Vulkan handles into static variables in utilities header
		initialize_utilities(physical_device, device.get(), queue, dispatch_loader, command_pool.get());
	}

	void initialize_command_buffers()
	{
		command_buffers = device->allocateCommandBuffersUnique(vk::CommandBufferAllocateInfo{ command_pool.get(), vk::CommandBufferLevel::ePrimary, static_cast<uint32_t>(framebuffers.size()) });
		LOG_DEBUG("Allocated [ " << command_buffers.size() << " ] command buffers");
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
#if defined(RAYTRACING)
	void initialize_offscreen_image()
	{
		// First, create the actual image
		auto image_create_info = vk::ImageCreateInfo{}
			.setArrayLayers(1)
			.setExtent(vk::Extent3D{ window_details.width, window_details.height, 1 })
			.setFormat(swapchain_details.image_format)
			.setImageType(vk::ImageType::e2D)
			.setMipLevels(1)
			.setTiling(vk::ImageTiling::eOptimal)
			.setUsage(vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eTransferSrc);

		auto image = device->createImageUnique(image_create_info);

		// Then, allocate memory for the image
		auto memory_requirements = device->getImageMemoryRequirements(image.get());
		auto memory_allocate_info = vk::MemoryAllocateInfo{ memory_requirements.size, find_memory_type(memory_requirements, vk::MemoryPropertyFlagBits::eDeviceLocal) };
		auto device_memory = device->allocateMemoryUnique(memory_allocate_info);

		device->bindImageMemory(image.get(), device_memory.get(), 0);

		// Finally, create an image view corresponding to this image
		auto image_view_create_info = vk::ImageViewCreateInfo{}
			.setImage(image.get())
			.setViewType(vk::ImageViewType::e2D)
			.setFormat(swapchain_details.image_format)
			.setSubresourceRange(get_single_layer_resource());

		auto image_view = device->createImageViewUnique(image_view_create_info);
		LOG_DEBUG("Successfully created offscreen image and image view");

		offscreen_image = Image{ std::move(image), std::move(device_memory), std::move(image_view) };
	}

	void initialize_shader_binding_table()
	{
		const uint32_t number_of_groups = 5;
		const uint32_t table_size = raytracing_properties.shaderHeaderSize * number_of_groups;

		shader_binding_table_buffer = create_buffer(table_size, vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eRaytracingNVX, vk::MemoryPropertyFlagBits::eHostVisible);

		// Write the shader handles into device memory 
		void* ptr = device->mapMemory(shader_binding_table_buffer.device_memory.get(), 0, table_size);
		device->getRaytracingShaderHandlesNVX(pipeline.get(), 0, number_of_groups, table_size, ptr, dispatch_loader);
		device->unmapMemory(shader_binding_table_buffer.device_memory.get());
		LOG_DEBUG("Successfully created shader binding table");
	}
#endif
	void initialize_scene()
	{
		scene.initialize();

		GeometryDefinition geom_0 = build_sphere();
		GeometryDefinition geom_1 = build_sphere(24, 24, 0.5f, { 1.5f, 0.5f, -1.5f });
		GeometryDefinition geom_2 = build_rect(4.0f, 4.0f, { 0.0f, 1.0f, 0.0f });
		
		scene.add_geometry(geom_0);
		scene.add_geometry(geom_1);
		scene.add_geometry(geom_2);
	}

	void initialize_descriptor_set()
	{
		// First, create the descriptor pool
		const std::vector<vk::DescriptorPoolSize> pool_sizes =
		{
			vk::DescriptorPoolSize{ vk::DescriptorType::eAccelerationStructureNVX, 1 },
			vk::DescriptorPoolSize{ vk::DescriptorType::eStorageImage, 1 },

			// Number of geometry meshes * 2
			vk::DescriptorPoolSize{ vk::DescriptorType::eStorageBuffer, static_cast<uint32_t>(scene.get_primitive_buffers().size()) * 2 } 
		};

		const uint32_t max_sets = 1;
		descriptor_pool = device->createDescriptorPoolUnique(vk::DescriptorPoolCreateInfo{ {}, max_sets, static_cast<uint32_t>(pool_sizes.size()), pool_sizes.data() });
		LOG_DEBUG("Successfully created descriptor pool");

		// Then, allocate descriptor sets
		descriptor_set = std::move(device->allocateDescriptorSetsUnique(vk::DescriptorSetAllocateInfo{ descriptor_pool.get(), 1, &descriptor_set_layout.get() })[0]);
		LOG_DEBUG("Successfully allocated descriptor set from descriptor pool");
	}

	void update_descriptor_sets()
	{
#if defined(RAYTRACING)
		// To write a descriptor, we need to provide:
		// - Descriptor set handle
		// - Binding
		// - Destination array element (usually 0)
		// - Count (usually 1)
		// - Type

		// Descriptor #0: top-level acceleration structure
		auto descriptor_accel_info = vk::DescriptorAccelerationStructureInfoNVX{ 1, &scene.get_tlas().inner.get() };
		auto write_descriptor_0 = vk::WriteDescriptorSet{ descriptor_set.get(), 0, 0, 1, vk::DescriptorType::eAccelerationStructureNVX };
		write_descriptor_0.setPNext(&descriptor_accel_info); // Notice that we write to pNext here!

		// Descriptor #1: offscreen storage image
		auto descriptor_image_info = vk::DescriptorImageInfo{ {}, offscreen_image.view.value().get(), vk::ImageLayout::eGeneral };
		auto write_descriptor_1 = vk::WriteDescriptorSet{ descriptor_set.get(), 1, 0, 1, vk::DescriptorType::eStorageImage };
		write_descriptor_1.setPImageInfo(&descriptor_image_info);

		// Descriptor #3: storage buffers for mesh normals
		std::vector<vk::DescriptorBufferInfo> normal_buffer_infos;
		for (size_t i = 0; i < scene.get_normal_buffers().size(); ++i)
		{
			normal_buffer_infos.push_back(vk::DescriptorBufferInfo{ scene.get_normal_buffers()[i].inner.get(), 0, VK_WHOLE_SIZE });
		}

		auto write_descriptor_2 = vk::WriteDescriptorSet{ 
			descriptor_set.get(), 
			2, 
			0,
			static_cast<uint32_t>(normal_buffer_infos.size()), 
			vk::DescriptorType::eStorageBuffer, 
			nullptr,
			normal_buffer_infos.data() };


		// Descriptor #4: storage buffers for mesh primitives
		std::vector<vk::DescriptorBufferInfo> primitive_buffer_infos;
		for (size_t i = 0; i < scene.get_primitive_buffers().size(); ++i)
		{
			primitive_buffer_infos.push_back(vk::DescriptorBufferInfo{ scene.get_primitive_buffers()[i].inner.get(), 0, VK_WHOLE_SIZE });
		}

		auto write_descriptor_3 = vk::WriteDescriptorSet{ 
			descriptor_set.get(),
			3, 
			0,
			static_cast<uint32_t>(primitive_buffer_infos.size()),
			vk::DescriptorType::eStorageBuffer,
			nullptr,
			primitive_buffer_infos.data() };

		// Gather write structs and update
		device->updateDescriptorSets({ write_descriptor_0, write_descriptor_1, write_descriptor_2, write_descriptor_3 }, {});
		LOG_DEBUG("Wrote to descriptor set");
#endif
	}

	void record_command_buffer(uint32_t index)
	{
		double cursor_x, cursor_y;
		glfwGetCursorPos(window_details.window, &cursor_x, &cursor_y);

		PushConstants push_constants =
		{
			static_cast<float>(window_details.width),
			static_cast<float>(window_details.height),
			static_cast<float>(cursor_x / window_details.width),
			static_cast<float>(cursor_y / window_details.height),
			get_elapsed_time()
		};

		const auto subresource = get_single_layer_resource();

		command_buffers[index]->begin(vk::CommandBufferBeginInfo{ vk::CommandBufferUsageFlagBits::eSimultaneousUse });
#if defined(RAYTRACING)
		image_barrier(command_buffers[index].get(),
					  offscreen_image.inner.get(),
					  subresource,
					  {},
					  vk::AccessFlagBits::eShaderWrite,
					  vk::ImageLayout::eUndefined,
					  vk::ImageLayout::eGeneral);

		command_buffers[index]->bindPipeline(vk::PipelineBindPoint::eRaytracingNVX, pipeline.get());
		command_buffers[index]->pushConstants(pipeline_layout.get(), vk::ShaderStageFlagBits::eRaygenNVX, 0, sizeof(PushConstants), &push_constants);
		command_buffers[index]->bindDescriptorSets(vk::PipelineBindPoint::eRaytracingNVX, pipeline_layout.get(), 0, descriptor_set.get(), {});
		command_buffers[index]->traceRaysNVX(shader_binding_table_buffer.inner.get(), 0,
											 shader_binding_table_buffer.inner.get(), 
											 raytracing_properties.shaderHeaderSize * 3, // 3 for ray generation group (1) and closest hit groups (2) 
											 raytracing_properties.shaderHeaderSize,
											 shader_binding_table_buffer.inner.get(), 
											 raytracing_properties.shaderHeaderSize * 1, // 1 for ray generation group 
										     raytracing_properties.shaderHeaderSize,
											 window_details.width, window_details.height, dispatch_loader);

		image_barrier(command_buffers[index].get(),
					  swapchain_images[index],
					  subresource,
					  {},
					  vk::AccessFlagBits::eTransferWrite,
					  vk::ImageLayout::eUndefined,
					  vk::ImageLayout::eTransferDstOptimal);

		image_barrier(command_buffers[index].get(),
					  offscreen_image.inner.get(),
					  subresource,
					  vk::AccessFlagBits::eShaderWrite,
					  vk::AccessFlagBits::eTransferRead,
					  vk::ImageLayout::eGeneral,
					  vk::ImageLayout::eTransferSrcOptimal);

		// Copy image contents
		const auto full_layer = vk::ImageSubresourceLayers{ vk::ImageAspectFlagBits::eColor, 0, 0, 1 };
		auto image_copy = vk::ImageCopy{}
			.setSrcSubresource(full_layer)
			.setDstSubresource(full_layer)
			.setExtent({ window_details.width, window_details.height, 1 });

		command_buffers[index]->copyImage(offscreen_image.inner.get(),
										  vk::ImageLayout::eTransferSrcOptimal,
										  swapchain_images[index],
										  vk::ImageLayout::eTransferDstOptimal,
										  image_copy);

		// Final barrier before viewing
		image_barrier(command_buffers[index].get(),
					  swapchain_images[index],
					  subresource,
					  vk::AccessFlagBits::eTransferWrite,
					  {},
					  vk::ImageLayout::eTransferDstOptimal,
					  vk::ImageLayout::ePresentSrcKHR);
#else
		const vk::ClearValue clear = std::array<float, 4>{ 0.0f, 0.0f, 0.0f, 1.0f };
		const vk::Rect2D render_area{ { 0, 0 }, swapchain_details.extent };

		command_buffers[index]->beginRenderPass(vk::RenderPassBeginInfo{ render_pass.get(), framebuffers[index].get(), render_area, 1, &clear }, vk::SubpassContents::eInline);
		command_buffers[index]->bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline.get());
		command_buffers[index]->pushConstants(pipeline_layout.get(), vk::ShaderStageFlagBits::eFragment, 0, sizeof(PushConstants), &push_constants);

		for (size_t geometry_index = 0; geometry_index < scene.get_number_of_instances(); ++geometry_index)
		{
			command_buffers[index]->bindVertexBuffers(0, scene.get_vertex_buffers()[geometry_index].inner.get(), vk::DeviceSize{ 0 });
			command_buffers[index]->bindIndexBuffer(scene.get_index_buffers()[geometry_index].inner.get(), 0, vk::IndexType::eUint32);
			// TODO: the index count should be stored elsewhere
			command_buffers[index]->drawIndexed(static_cast<uint32_t>(geometry_def.indices.size()), 1, 0, 0, 0);
		}

		command_buffers[index]->endRenderPass();
#endif
		command_buffers[index]->end();
	}

	void draw()
	{
		while (!glfwWindowShouldClose(window_details.window))
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

	WindowDetails window_details;
	SurfaceDetails surface_details;
	SwapchainDetails swapchain_details;

	vk::DebugReportCallbackEXT debug_report_callback;
	vk::PhysicalDevice physical_device;
	vk::Queue queue;
	uint32_t queue_family_index;

	vk::UniqueInstance instance;
	vk::DispatchLoaderDynamic dispatch_loader;
	vk::UniqueDevice device;
	vk::UniqueSurfaceKHR surface;
	vk::UniqueSwapchainKHR swapchain;
	std::vector<vk::Image> swapchain_images;
	std::vector<vk::UniqueImageView> swapchain_image_views;
	vk::UniqueRenderPass render_pass;
	std::vector<vk::UniqueFramebuffer> framebuffers;

	vk::UniqueCommandPool command_pool;
	std::vector<vk::UniqueCommandBuffer> command_buffers;
	std::vector<vk::UniqueFence> fences;
	vk::UniqueSemaphore semaphore_image_available;
	vk::UniqueSemaphore sempahore_render_finished;

	Buffer shader_binding_table_buffer;

	vk::UniqueDescriptorSetLayout descriptor_set_layout;
	vk::UniquePipelineLayout pipeline_layout;
#if defined(RAYTRACING)
	Image offscreen_image;
	vk::PhysicalDeviceRaytracingPropertiesNVX raytracing_properties;
	vk::UniqueHandle<vk::Pipeline, vk::DispatchLoaderDynamic> pipeline;
#else
	// The additional dispatch loader template parameter is not needed for a graphics pipeline
	vk::UniquePipeline pipeline;
#endif
	Scene scene;
	vk::UniqueDescriptorPool descriptor_pool;
	vk::UniqueDescriptorSet  descriptor_set;
};

int main()
{
	Application app{ 800, 600, "vkstarter" };
	app.draw();

	return EXIT_SUCCESS;
}