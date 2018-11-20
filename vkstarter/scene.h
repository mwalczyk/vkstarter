#pragma once

#include "utilities.h"

#include "gtc/type_ptr.hpp"

class GeometryInstance
{
public:

	GeometryInstance(const GeometryDefinition& geometry_def) : 
		geometry_def{ geometry_def } 
	{
		create_buffers();
		create_acceleration_structure();
	}

	void create_buffers()
	{
		// Describe how the memory associated with these buffers will be accessed
		auto memory_properties = vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible;

		// Describe the intended usage of these buffers
		auto vertex_buffer_usage = vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eRaytracingNVX;
		auto index_buffer_usage = vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eRaytracingNVX;

		// Create the buffers (and device memory)
		vertex_buffer = create_buffer(sizeof(geometry_def.vertices[0]) * geometry_def.vertices.size(), vertex_buffer_usage, memory_properties);
		index_buffer = create_buffer(sizeof(geometry_def.indices[0]) * geometry_def.indices.size(), index_buffer_usage, memory_properties);
		LOG_DEBUG("Created vertex and index buffers");

		upload(vertex_buffer, geometry_def.vertices);
		upload(index_buffer, geometry_def.indices);
		LOG_DEBUG("Uploaded vertex and index data to buffers");
	}

	void create_acceleration_structure()
	{
		// First, build the geometry
		auto geometry_triangles = vk::GeometryTrianglesNVX{}
			.setIndexCount(static_cast<uint32_t>(geometry_def.indices.size()))
			.setIndexData(index_buffer.inner.get())
			.setIndexType(vk::IndexType::eUint32)
			.setVertexCount(static_cast<uint32_t>(geometry_def.vertices.size()))
			.setVertexData(vertex_buffer.inner.get())
			.setVertexFormat(vk::Format::eR32G32B32Sfloat)
			.setVertexStride(sizeof(geometry_def.vertices[0]));

		auto geometry_data = vk::GeometryDataNVX{ geometry_triangles };
		geometry = vk::GeometryNVX{ vk::GeometryTypeNVX::eTriangles, geometry_data, vk::GeometryFlagBitsNVX::eOpaque };
	
		// Then, build the bottom-level acceleration structure for this geometry instance
		bottom_level = build_accel(vk::AccelerationStructureTypeNVX::eBottomLevel, geometry, 0);
	}

	const std::vector<glm::vec3>& get_vertices() const { return geometry_def.vertices; }
	const std::vector<glm::vec3>& get_normals() const { return geometry_def.normals; }
	const std::vector<uint32_t>& get_indices() const { return geometry_def.indices; }
	
	const vk::GeometryNVX& get_geometry() const { return geometry; }

private:

	GeometryDefinition geometry_def;

	vk::GeometryNVX geometry;

	Buffer vertex_buffer;
	Buffer normal_buffer;
	Buffer index_buffer;

	AccelerationStructure bottom_level;

	friend class Scene;
};

class Scene
{
public:

	Scene(size_t capacity = max_instances)
	{
		// Create a buffer to hold the instance data
		size_t instance_buffer_size = sizeof(VkGeometryInstance) * capacity;
		instances_buffer = create_buffer(instance_buffer_size, vk::BufferUsageFlagBits::eRaytracingNVX, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
	}

	void add_instance(const GeometryInstance& geometry_instance, const glm::mat4x3& transform)
	{
		if (number_of_instances < max_instances)
		{
			// Describe the instance and its transform
			VkGeometryInstance instance;
			std::memcpy(instance.transform, glm::value_ptr(transform), sizeof(glm::mat4x3));
			instance.instanceId = number_of_instances; // Starts at 0 and increments...
			instance.mask = 0xff;
			instance.instanceOffset = 0;
			instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_CULL_DISABLE_BIT_NVX;
			instance.accelerationStructureHandle = geometry_instance.bottom_level.handle;
			const std::vector<VkGeometryInstance> instances = { instance };

			// Upload the transform data to the GPU
			size_t offset = sizeof(instance) * number_of_instances;
			upload(instances_buffer, instances, offset);

			// Keep track of the transform data on the CPU, as well
			transforms.push_back(transform);

			// Keep track of how many instances we've allocated
			number_of_instances++;

			// Update scratch memory size (if necessary)
			size_t potential_size = std::max(geometry_instance.bottom_level.scratch_memory_requirements.memoryRequirements.size,
										     top_level.scratch_memory_requirements.memoryRequirements.size);

			if (potential_size > scratch_memory_size)
			{
				scratch_memory_size = potential_size;

				scratch_buffer.inner.reset();
				scratch_buffer.device_memory.reset();
				scratch_buffer = create_buffer(scratch_memory_size, vk::BufferUsageFlagBits::eRaytracingNVX, vk::MemoryPropertyFlagBits::eDeviceLocal);
				LOG_DEBUG("Updating TLAS scratch memory size to: " << scratch_memory_size);
			}

			// Update the TLAS
			create_acceleration_structure(geometry_instance);
		}
	}

	void create_acceleration_structure(const GeometryInstance& geometry_instance, bool update = false)
	{
		LOG_DEBUG("Building TLAS with " << number_of_instances << " instances");
		top_level = build_accel(vk::AccelerationStructureTypeNVX::eTopLevel, {}, number_of_instances);

		single_time_commands([&](vk::CommandBuffer command_buffer) {
			auto memory_barrier = vk::MemoryBarrier{ vk::AccessFlagBits::eAccelerationStructureWriteNVX | vk::AccessFlagBits::eAccelerationStructureReadNVX,
													 vk::AccessFlagBits::eAccelerationStructureWriteNVX | vk::AccessFlagBits::eAccelerationStructureReadNVX };

			// Build bottom-level acceleration structure
			command_buffer.buildAccelerationStructureNVX(
				geometry_instance.bottom_level.type,
				0,
				{},
				0,
				geometry_instance.geometry,
				{},
				false,
				geometry_instance.bottom_level.inner.get(),
				{},
				scratch_buffer.inner.get(),
				0,
				dispatch_loader);

			command_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eRaytracingNVX, vk::PipelineStageFlagBits::eRaytracingNVX, {}, memory_barrier, {}, {});

			// Build top-level acceleration structure
			command_buffer.buildAccelerationStructureNVX(
				top_level.type,
				1,
				instances_buffer.inner.get(),
				0,
				{},
				{},
				false,
				top_level.inner.get(),
				{},
				scratch_buffer.inner.get(),
				0,
				dispatch_loader);

			command_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eRaytracingNVX, vk::PipelineStageFlagBits::eRaytracingNVX, {}, memory_barrier, {}, {});

		});
	}

private:

	static const size_t max_instances = 256;
	size_t number_of_instances = 0;
	size_t scratch_memory_size = 0; // Updated every time a new BLAS is added (max)

	std::vector<glm::mat4> transforms; // CPU

	Buffer instances_buffer; // GPU
	Buffer scratch_buffer;

	AccelerationStructure top_level;
};