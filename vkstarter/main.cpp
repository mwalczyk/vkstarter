#include <iostream>
#include <sstream>
#include <fstream>
#include <limits>

#define VK_USE_PLATFORM_WIN32_KHR
#define GLFW_EXPOSE_NATIVE_WIN32

#include "glfw3.h"
#include "glfw3native.h"

#include "vulkan/vulkan.hpp"

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

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(VkDebugReportFlagsEXT flags,
													 VkDebugReportObjectTypeEXT object_type,
													 uint64_t object,
													 size_t location,
													 int32_t code,
													 const char* layer_prefix,
													 const char* msg,
													 void* user_data) 
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

int main()
{
	const std::string app_name = "vkstarter";
	const uint32_t width = 800;
	const uint32_t height = 600;
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
	auto* window = glfwCreateWindow(width, height, app_name.c_str(), nullptr, nullptr);

	// Instance
	std::vector<const char*> layers;
	std::vector<const char*> extensions{ VK_EXT_DEBUG_REPORT_EXTENSION_NAME, VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_WIN32_SURFACE_EXTENSION_NAME };
#ifdef _DEBUG
	layers.push_back("VK_LAYER_LUNARG_standard_validation");
#endif
	auto application_info = vk::ApplicationInfo{ app_name.c_str(), VK_MAKE_VERSION(1, 0, 0), app_name.c_str(), VK_MAKE_VERSION(1, 0, 0), VK_API_VERSION_1_1 };
	auto instance_create_info = vk::InstanceCreateInfo{};
	instance_create_info.pApplicationInfo = &application_info;
	instance_create_info.ppEnabledLayerNames = layers.data();
	instance_create_info.enabledLayerCount = static_cast<uint32_t>(layers.size());
	instance_create_info.ppEnabledExtensionNames = extensions.data();
	instance_create_info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());

	auto instance = vk::createInstanceUnique(instance_create_info);
	
	// Debug report callback
#ifdef _DEBUG
	auto dynamic_dispatch_loader = vk::DispatchLoaderDynamic{ instance.get() };
	auto debug_report_callback_create_info = vk::DebugReportCallbackCreateInfoEXT{};
	debug_report_callback_create_info.flags = vk::DebugReportFlagBitsEXT::eError | vk::DebugReportFlagBitsEXT::eWarning;
	debug_report_callback_create_info.pfnCallback = debug_callback;
	
	auto debug_report_callback = instance->createDebugReportCallbackEXT(debug_report_callback_create_info, nullptr, dynamic_dispatch_loader);
	std::cout << "Initializing debug report callback\n";
#endif

	// Physical device
	auto physical_devices = instance->enumeratePhysicalDevices();
	assert(!physical_devices.empty());

	auto physical_device = physical_devices[0];

	auto queue_family_properties = physical_device.getQueueFamilyProperties();

	const float priority = 0.0f;
	auto queue_create_info = vk::DeviceQueueCreateInfo{};
	queue_create_info.pQueuePriorities = &priority;
	queue_create_info.queueCount = 1;
	queue_create_info.queueFamilyIndex = static_cast<uint32_t>(std::distance(queue_family_properties.begin(), std::find_if(queue_family_properties.begin(), queue_family_properties.end(),
			[](const vk::QueueFamilyProperties& item) { return item.queueFlags & vk::QueueFlagBits::eGraphics; }
	)));
	std::cout << "Queue family at index [ " << queue_create_info.queueFamilyIndex << " ] supports graphics\n";

	// Logical device
	const std::vector<const char*> device_extensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
	auto device_create_info = vk::DeviceCreateInfo{};
	device_create_info.pQueueCreateInfos = &queue_create_info;
	device_create_info.queueCreateInfoCount = 1;
	device_create_info.ppEnabledExtensionNames = device_extensions.data();
	device_create_info.enabledExtensionCount = static_cast<uint32_t>(device_extensions.size());

	auto device = physical_device.createDeviceUnique(device_create_info);

	// Queue
	auto queue = device->getQueue(queue_create_info.queueFamilyIndex, 0);

	// Surface
	auto surface_create_info = vk::Win32SurfaceCreateInfoKHR{ {}, GetModuleHandle(nullptr), glfwGetWin32Window(window) };

	auto surface = instance->createWin32SurfaceKHRUnique(surface_create_info);

	auto surface_capabilities = physical_device.getSurfaceCapabilitiesKHR(surface.get());
	auto surface_formats = physical_device.getSurfaceFormatsKHR(surface.get());
	auto surface_present_modes = physical_device.getSurfacePresentModesKHR(surface.get());
	auto surface_support = physical_device.getSurfaceSupportKHR(queue_create_info.queueFamilyIndex, surface.get());

	// Swapchain
	const auto swapchain_image_format = vk::Format::eB8G8R8A8Unorm;
	const vk::Extent2D swapchain_extent = { width, height };
	auto swapchain_create_info = vk::SwapchainCreateInfoKHR{};
	swapchain_create_info.presentMode = vk::PresentModeKHR::eMailbox;
	swapchain_create_info.imageExtent = swapchain_extent;
	swapchain_create_info.imageFormat = swapchain_image_format;
	swapchain_create_info.imageArrayLayers = 1;
	swapchain_create_info.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;
	swapchain_create_info.minImageCount = surface_capabilities.minImageCount + 1;
	swapchain_create_info.preTransform = surface_capabilities.currentTransform;
	swapchain_create_info.clipped = true;
	swapchain_create_info.surface = surface.get();

	auto swapchain = device->createSwapchainKHRUnique(swapchain_create_info);

	auto swapchain_images = device->getSwapchainImagesKHR(swapchain.get());
	std::cout << "There are [ " << swapchain_images.size() << " ] images in the swapchain\n";

	std::vector<vk::UniqueImageView> swapchain_image_views;
	for (const auto& image : swapchain_images)
	{
		auto image_view_create_info = vk::ImageViewCreateInfo{};
		image_view_create_info.image = image;
		image_view_create_info.viewType = vk::ImageViewType::e2D;
		image_view_create_info.format = swapchain_image_format;
		image_view_create_info.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
		image_view_create_info.subresourceRange.levelCount = 1;
		image_view_create_info.subresourceRange.layerCount = 1;

		swapchain_image_views.push_back(device->createImageViewUnique(image_view_create_info));
	}
	std::cout << "Created [ " << swapchain_image_views.size() << " ] image views\n";

	// Shader modules 
	auto vs_module = load_spv_into_module(device, "vert.spv");
	auto fs_module = load_spv_into_module(device, "frag.spv");
	std::cout << "Successfully loaded shader modules\n";

	// Pipeline layout
	auto pipeline_layout = device->createPipelineLayoutUnique({});

	// Render pass
	auto attachment_description = vk::AttachmentDescription{};
	attachment_description.format = swapchain_image_format;
	attachment_description.loadOp = vk::AttachmentLoadOp::eClear;
	attachment_description.storeOp = vk::AttachmentStoreOp::eStore;
	attachment_description.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
	attachment_description.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
	attachment_description.finalLayout = vk::ImageLayout::ePresentSrcKHR;

	const uint32_t attachment_index = 0;
	auto attachment_reference = vk::AttachmentReference{ attachment_index, vk::ImageLayout::eColorAttachmentOptimal };
	
	auto subpass_description = vk::SubpassDescription{};
	subpass_description.pColorAttachments = &attachment_reference;
	subpass_description.colorAttachmentCount = 1;

	auto subpass_dependency = vk::SubpassDependency{};
	subpass_dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	subpass_dependency.dstSubpass = 0;
	subpass_dependency.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
	subpass_dependency.srcAccessMask = {};
	subpass_dependency.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
	subpass_dependency.dstAccessMask = vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;

	auto render_pass_create_info = vk::RenderPassCreateInfo{};
	render_pass_create_info.pAttachments = &attachment_description;
	render_pass_create_info.attachmentCount = 1;
	render_pass_create_info.pSubpasses = &subpass_description;
	render_pass_create_info.subpassCount = 1;
	render_pass_create_info.pDependencies = &subpass_dependency;
	render_pass_create_info.dependencyCount = 1;

	auto render_pass = device->createRenderPassUnique(render_pass_create_info);

	// Graphics pipeline
	const char* entry_point = "main";
	auto vs_stage_create_info = vk::PipelineShaderStageCreateInfo{ {}, vk::ShaderStageFlagBits::eVertex, vs_module.get(), entry_point };
	auto fs_stage_create_info = vk::PipelineShaderStageCreateInfo{ {}, vk::ShaderStageFlagBits::eFragment, fs_module.get(), entry_point };
	const vk::PipelineShaderStageCreateInfo shader_stage_create_infos[2] = { vs_stage_create_info, fs_stage_create_info };

	auto vertex_input_state_create_info = vk::PipelineVertexInputStateCreateInfo{};
	auto input_assembly_create_info = vk::PipelineInputAssemblyStateCreateInfo{ {}, vk::PrimitiveTopology::eTriangleList };

	const vk::Viewport viewport{ 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f };
	const vk::Rect2D scissor{ { 0, 0 }, swapchain_extent };
	auto viewport_state_create_info = vk::PipelineViewportStateCreateInfo{};
	viewport_state_create_info.pViewports = &viewport;
	viewport_state_create_info.viewportCount = 1;
	viewport_state_create_info.pScissors = &scissor;
	viewport_state_create_info.scissorCount = 1;

	auto rasterization_state_create_info = vk::PipelineRasterizationStateCreateInfo{};
	rasterization_state_create_info.frontFace = vk::FrontFace::eClockwise;
	rasterization_state_create_info.cullMode = vk::CullModeFlagBits::eBack;
	rasterization_state_create_info.lineWidth = 1.0f;

	auto multisample_state_create_info = vk::PipelineMultisampleStateCreateInfo{};

	auto color_blend_attachment_state = vk::PipelineColorBlendAttachmentState{};
	color_blend_attachment_state.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;

	auto color_blend_state_create_info = vk::PipelineColorBlendStateCreateInfo{};
	color_blend_state_create_info.attachmentCount = 1;
	color_blend_state_create_info.pAttachments = &color_blend_attachment_state;

	auto graphics_pipeline_create_info = vk::GraphicsPipelineCreateInfo{};
	graphics_pipeline_create_info.layout = pipeline_layout.get();
	graphics_pipeline_create_info.renderPass = render_pass.get();
	graphics_pipeline_create_info.pStages = shader_stage_create_infos;
	graphics_pipeline_create_info.stageCount = 2;
	graphics_pipeline_create_info.pVertexInputState = &vertex_input_state_create_info;
	graphics_pipeline_create_info.pInputAssemblyState = &input_assembly_create_info;
	graphics_pipeline_create_info.pViewportState = &viewport_state_create_info;
	graphics_pipeline_create_info.pRasterizationState = &rasterization_state_create_info;
	graphics_pipeline_create_info.pMultisampleState = &multisample_state_create_info;
	graphics_pipeline_create_info.pColorBlendState = &color_blend_state_create_info;

	auto graphics_pipeline = device->createGraphicsPipelineUnique({}, graphics_pipeline_create_info);
	std::cout << "Created graphics pipeline\n";

	// Framebuffers
	std::vector<vk::UniqueFramebuffer> framebuffers;
	for (const auto& image_view : swapchain_image_views)
	{
		auto framebuffer_create_info = vk::FramebufferCreateInfo{};
		framebuffer_create_info.pAttachments = &image_view.get();
		framebuffer_create_info.attachmentCount = 1;
		framebuffer_create_info.renderPass = render_pass.get();
		framebuffer_create_info.width = width;
		framebuffer_create_info.height = height;
		framebuffer_create_info.layers = 1;
		std::cout << "Creating framebuffer\n";

		framebuffers.push_back(device->createFramebufferUnique(framebuffer_create_info));
	}
	
	// Command pool
	auto command_pool = device->createCommandPoolUnique(vk::CommandPoolCreateInfo{ {}, queue_create_info.queueFamilyIndex });

	// Command buffers
	auto command_buffers = device->allocateCommandBuffersUnique(vk::CommandBufferAllocateInfo{ command_pool.get(), vk::CommandBufferLevel::ePrimary, static_cast<uint32_t>(framebuffers.size()) });
	std::cout << "Allocated [ " << command_buffers.size() << " ] command buffers\n";

	const vk::ClearValue clear = std::array<float, 4>{ 0.0f, 0.0f, 0.0f, 1.0f };
	const vk::Rect2D render_area = { { 0, 0 }, swapchain_extent };
	for (size_t i = 0; i < command_buffers.size(); ++i)
	{	
		command_buffers[i]->begin(vk::CommandBufferBeginInfo{ vk::CommandBufferUsageFlagBits::eSimultaneousUse });
		command_buffers[i]->beginRenderPass(vk::RenderPassBeginInfo{ render_pass.get(), framebuffers[i].get(), render_area, 1, &clear }, vk::SubpassContents::eInline);
		command_buffers[i]->bindPipeline(vk::PipelineBindPoint::eGraphics, graphics_pipeline.get());
		command_buffers[i]->draw(3, 1, 0, 0);
		command_buffers[i]->endRenderPass();
		command_buffers[i]->end();
	}

	// Semaphores
	auto semaphore_image_available = device->createSemaphoreUnique({});
	auto sempahore_render_finished = device->createSemaphoreUnique({});

	// Draw loop
	while (!glfwWindowShouldClose(window)) 
	{
		glfwPollEvents();

		auto index = device->acquireNextImageKHR(swapchain.get(), (std::numeric_limits<uint64_t>::max)(), semaphore_image_available.get(), {});

		vk::PipelineStageFlags wait_stages[] = { vk::PipelineStageFlagBits::eColorAttachmentOutput };
		auto submit_info = vk::SubmitInfo{};
		submit_info.pWaitSemaphores = &semaphore_image_available.get();
		submit_info.waitSemaphoreCount = 1;
		submit_info.pWaitDstStageMask = wait_stages;
		submit_info.pSignalSemaphores = &sempahore_render_finished.get();
		submit_info.signalSemaphoreCount = 1;
		submit_info.pCommandBuffers = &command_buffers[index.value].get();
		submit_info.commandBufferCount = 1;

		queue.submit(submit_info, {});

		auto present_info = vk::PresentInfoKHR{};
		present_info.pWaitSemaphores = &sempahore_render_finished.get();
		present_info.waitSemaphoreCount = 1;
		present_info.pSwapchains = &swapchain.get();
		present_info.swapchainCount = 1;
		present_info.pImageIndices = &index.value;

		queue.presentKHR(present_info);
	}

	// Wait for all work on the GPU to finish
	device->waitIdle();

	// Clean up GLFW objects
	glfwDestroyWindow(window);
	glfwTerminate();

	return EXIT_SUCCESS;
}