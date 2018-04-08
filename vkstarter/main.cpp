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
	
	auto instance = vk::createInstanceUnique(vk::InstanceCreateInfo{ {}, &application_info, static_cast<uint32_t>(layers.size()), layers.data(), static_cast<uint32_t>(extensions.size()), extensions.data() });
	
	// Debug report callback
#ifdef _DEBUG
	auto dynamic_dispatch_loader = vk::DispatchLoaderDynamic{ instance.get() };
	auto debug_report_callback_create_info = vk::DebugReportCallbackCreateInfoEXT{ vk::DebugReportFlagBitsEXT::eError | vk::DebugReportFlagBitsEXT::eWarning, debug_callback };
	
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
	std::cout << "Using queue family at index [ " << queue_create_info.queueFamilyIndex << " ], which supports graphics operations\n";

	// Logical device
	const std::vector<const char*> device_extensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
	auto device_create_info = vk::DeviceCreateInfo{}
		.setPQueueCreateInfos(&queue_create_info)
		.setQueueCreateInfoCount(1)
		.setPpEnabledExtensionNames(device_extensions.data())
		.setEnabledExtensionCount(static_cast<uint32_t>(device_extensions.size()));

	auto device = physical_device.createDeviceUnique(device_create_info);

	// Queue
	const uint32_t queue_index = 0;
	auto queue = device->getQueue(queue_create_info.queueFamilyIndex, queue_index);

	// Surface
	auto surface_create_info = vk::Win32SurfaceCreateInfoKHR{ {}, GetModuleHandle(nullptr), glfwGetWin32Window(window) };

	auto surface = instance->createWin32SurfaceKHRUnique(surface_create_info);

	auto surface_capabilities = physical_device.getSurfaceCapabilitiesKHR(surface.get());
	auto surface_formats = physical_device.getSurfaceFormatsKHR(surface.get());
	auto surface_present_modes = physical_device.getSurfacePresentModesKHR(surface.get());
	auto surface_support = physical_device.getSurfaceSupportKHR(queue_create_info.queueFamilyIndex, surface.get());

	// Swapchain
	const auto swapchain_image_format{ vk::Format::eB8G8R8A8Unorm };
	const auto swapchain_extent = vk::Extent2D{ width, height };
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

	auto swapchain = device->createSwapchainKHRUnique(swapchain_create_info);

	auto swapchain_images = device->getSwapchainImagesKHR(swapchain.get());
	std::cout << "There are [ " << swapchain_images.size() << " ] images in the swapchain\n";

	std::vector<vk::UniqueImageView> swapchain_image_views;
	for (const auto& image : swapchain_images)
	{
		const auto subresource_range = vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };
		auto image_view_create_info = vk::ImageViewCreateInfo{ {}, image, vk::ImageViewType::e2D, swapchain_image_format, {}, subresource_range };
		swapchain_image_views.push_back(device->createImageViewUnique(image_view_create_info));
	}
	std::cout << "Created [ " << swapchain_image_views.size() << " ] image views\n";

	// Shader modules 
	const std::string path_prefix = "";
	const std::string vs_spv_path = path_prefix + "vert.spv";
	const std::string fs_spv_path = path_prefix + "frag.spv";
	auto vs_module = load_spv_into_module(device, vs_spv_path);
	auto fs_module = load_spv_into_module(device, fs_spv_path);
	std::cout << "Successfully loaded shader modules\n";

	// Pipeline layout
	auto pipeline_layout = device->createPipelineLayoutUnique({ /* Add push constants and descriptor sets here */ });

	// Render pass
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

	auto render_pass = device->createRenderPassUnique(render_pass_create_info);

	// Graphics pipeline
	const char* entry_point = "main";
	auto vs_stage_create_info = vk::PipelineShaderStageCreateInfo{ {}, vk::ShaderStageFlagBits::eVertex, vs_module.get(), entry_point };
	auto fs_stage_create_info = vk::PipelineShaderStageCreateInfo{ {}, vk::ShaderStageFlagBits::eFragment, fs_module.get(), entry_point };
	const vk::PipelineShaderStageCreateInfo shader_stage_create_infos[2] = { vs_stage_create_info, fs_stage_create_info };

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

	auto graphics_pipeline = device->createGraphicsPipelineUnique({}, graphics_pipeline_create_info);
	std::cout << "Created graphics pipeline\n";

	// Framebuffers
	const uint32_t framebuffer_layers = 1;
	std::vector<vk::UniqueFramebuffer> framebuffers;
	for (const auto& image_view : swapchain_image_views)
	{
		auto framebuffer_create_info = vk::FramebufferCreateInfo{ {}, render_pass.get(), 1, &image_view.get(), width, height, framebuffer_layers };
		framebuffers.push_back(device->createFramebufferUnique(framebuffer_create_info));
	}
	std::cout << "Created [ " << framebuffers.size() << " ] framebuffers\n";

	// Command pool
	auto command_pool = device->createCommandPoolUnique(vk::CommandPoolCreateInfo{ {}, queue_create_info.queueFamilyIndex });

	// Command buffers
	auto command_buffers = device->allocateCommandBuffersUnique(vk::CommandBufferAllocateInfo{ command_pool.get(), vk::CommandBufferLevel::ePrimary, static_cast<uint32_t>(framebuffers.size()) });
	std::cout << "Allocated [ " << command_buffers.size() << " ] command buffers\n";

	const vk::ClearValue clear = std::array<float, 4>{ 0.15f, 0.15f, 0.15f, 1.0f };
	const vk::Rect2D render_area{ { 0, 0 }, swapchain_extent };
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

		const vk::PipelineStageFlags wait_stages[] = { vk::PipelineStageFlagBits::eColorAttachmentOutput };

		auto submit_info = vk::SubmitInfo{ 1, &semaphore_image_available.get(), wait_stages, 1, &command_buffers[index.value].get(), 1, &sempahore_render_finished.get() };
		queue.submit(submit_info, {});

		auto present_info = vk::PresentInfoKHR{ 1, &sempahore_render_finished.get(), 1, &swapchain.get(), &index.value };
		queue.presentKHR(present_info);
	}

	// Wait for all work on the GPU to finish
	device->waitIdle();

	// Clean up GLFW objects
	glfwDestroyWindow(window);
	glfwTerminate();

	return EXIT_SUCCESS;
}