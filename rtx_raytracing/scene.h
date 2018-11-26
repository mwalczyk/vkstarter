#pragma once

#include "utilities.h"

class Scene
{
public:

	Scene() : number_of_unique_geometries{ 0 } {}

	const AccelerationStructure& get_tlas() const { return top_level; }
	const std::vector<Buffer>& get_vertex_buffers() const { return vertex_buffers; }
	const std::vector<Buffer>& get_normal_buffers() const { return normal_buffers; }
	const std::vector<Buffer>& get_index_buffers() const { return index_buffers; }
	const std::vector<Buffer>& get_primitive_buffers() const { return primitive_buffers; }
	size_t get_number_of_instances() const { return transforms.size(); }
	size_t get_number_of_unique_geometries() const { return number_of_unique_geometries; }

	std::vector<vk::DescriptorBufferInfo> get_normal_buffer_infos() const
	{
		std::vector<vk::DescriptorBufferInfo> buffer_infos;
		for (size_t i = 0; i < normal_buffers.size(); ++i)
		{
			buffer_infos.push_back(vk::DescriptorBufferInfo{ normal_buffers[i].inner.get(), 0, VK_WHOLE_SIZE });
		}

		return buffer_infos;
	}

	std::vector<vk::DescriptorBufferInfo> get_primitive_buffer_infos() const
	{
		std::vector<vk::DescriptorBufferInfo> buffer_infos;
		for (size_t i = 0; i < primitive_buffers.size(); ++i)
		{
			buffer_infos.push_back(vk::DescriptorBufferInfo{ primitive_buffers[i].inner.get(), 0, VK_WHOLE_SIZE });
		}

		return buffer_infos;
	}

	void initialize(size_t capacity = max_instances)
	{
		// Create a buffer to hold the instance data
		size_t instance_buffer_size = sizeof(VkGeometryInstance) * capacity;
		instances_buffer = create_buffer(instance_buffer_size, vk::BufferUsageFlagBits::eRaytracingNVX, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
	}

	void add_geometry(const GeometryDefinition& geometry_def, const std::vector<glm::mat4x3>& instance_transforms = { get_identity_matrix() })
	{
		// Describe how the memory associated with these buffers will be accessed
		const auto memory_properties = vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible;

		// Describe the intended usage of these buffers
		const auto vertex_buffer_usage = vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eRaytracingNVX;
		const auto index_buffer_usage = vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eRaytracingNVX;
		const auto normal_buffer_usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eRaytracingNVX;
		const auto primitive_buffer_usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eRaytracingNVX;

		// Create the buffers (and device memory)
		Buffer vertex_buffer = create_buffer(sizeof(geometry_def.vertices[0]) * geometry_def.vertices.size(), vertex_buffer_usage, memory_properties);
		Buffer index_buffer = create_buffer(sizeof(geometry_def.indices[0]) * geometry_def.indices.size(), index_buffer_usage, memory_properties);
		Buffer normal_buffer = create_buffer(sizeof(geometry_def.normals[0]) * geometry_def.normals.size(), normal_buffer_usage, memory_properties);
		Buffer primitive_buffer = create_buffer(sizeof(geometry_def.primitives[0]) * geometry_def.primitives.size(), primitive_buffer_usage, memory_properties);
		LOG_DEBUG("Created vertex, normal, index, and primitive buffers");

		upload(vertex_buffer, geometry_def.vertices);
		upload(normal_buffer, geometry_def.normals);
		upload(index_buffer, geometry_def.indices);
		upload(primitive_buffer, geometry_def.primitives);
		LOG_DEBUG("Uploaded attribute data to buffers");

		// Push back buffers
		vertex_buffers.push_back(std::move(vertex_buffer));
		normal_buffers.push_back(std::move(normal_buffer));
		index_buffers.push_back(std::move(index_buffer));
		primitive_buffers.push_back(std::move(primitive_buffer));

		// Then, build the geometry - because we move the buffers into a vector (above), 
		// we need to be sure to reference the buffer handles from inside the vector
		auto geometry_triangles = vk::GeometryTrianglesNVX{}
			.setIndexCount(static_cast<uint32_t>(geometry_def.indices.size()))
			.setIndexData(index_buffers.back().inner.get())
			.setIndexType(vk::IndexType::eUint32)
			.setVertexCount(static_cast<uint32_t>(geometry_def.vertices.size()))
			.setVertexData(vertex_buffers.back().inner.get())
			.setVertexFormat(vk::Format::eR32G32B32Sfloat)
			.setVertexStride(sizeof(geometry_def.vertices[0]));

		geometries.push_back(vk::GeometryNVX{ vk::GeometryTypeNVX::eTriangles, vk::GeometryDataNVX{ geometry_triangles }, vk::GeometryFlagBitsNVX::eOpaque });

		// Finally, build the bottom-level acceleration structure for this geometry instance
		AccelerationStructure bottom_level = build_accel(vk::AccelerationStructureTypeNVX::eBottomLevel, geometries.back(), 0);
		bottom_levels.push_back(std::move(bottom_level));

		// Add a new instance with the given transform and update the TLAS
		add_instance(bottom_levels.back().handle, instance_transforms);
	}

private:

	void add_instance(uint64_t handle, const std::vector<glm::mat4x3>& instance_transforms)
	{
		if (transforms.size() < max_instances)
		{
			// Upload the transform data to the GPU
			const size_t offset = sizeof(VkGeometryInstance) * transforms.size();

			std::vector<VkGeometryInstance> instances;

			for (const auto& transform : instance_transforms)
			{
				// Describe the instance and its transform
				VkGeometryInstance instance;
				std::memcpy(instance.transform, glm::value_ptr(transform), sizeof(glm::mat4x3));
				instance.instanceId = number_of_unique_geometries; // Starts at 0 and increments...
				instance.mask = 0xff;
				instance.instanceOffset = 0;
				instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_CULL_DISABLE_BIT_NVX;
				instance.accelerationStructureHandle = handle;
				instances.push_back(instance);

				// Keep track of the transform data on the CPU, as it may change and need to be
				// re-uploaded to the GPU
				transforms.push_back(transform);
			}
			upload(instances_buffer, instances, offset);

			number_of_unique_geometries++;

			// Update (or create) the TLAS
			update_tlas();
		}
	}

	void update_tlas(bool update = false)
	{
		LOG_DEBUG("Building TLAS with " << transforms.size() << " instances");
		top_level = build_accel(vk::AccelerationStructureTypeNVX::eTopLevel, {}, static_cast<uint32_t>(transforms.size()));

		// Update scratch memory size (if necessary)
		const size_t potential_size = std::max(bottom_levels.back().scratch_memory_requirements.memoryRequirements.size,
										       top_level.scratch_memory_requirements.memoryRequirements.size);

		if (potential_size > scratch_memory_size)
		{
			scratch_memory_size = potential_size;

			scratch_buffer = create_buffer(scratch_memory_size, vk::BufferUsageFlagBits::eRaytracingNVX, vk::MemoryPropertyFlagBits::eDeviceLocal);
			LOG_DEBUG("Updating TLAS scratch memory size to: " << scratch_memory_size);
		}

		single_time_commands([&](vk::CommandBuffer command_buffer) {
			const auto memory_barrier = vk::MemoryBarrier{ vk::AccessFlagBits::eAccelerationStructureWriteNVX | vk::AccessFlagBits::eAccelerationStructureReadNVX,
														   vk::AccessFlagBits::eAccelerationStructureWriteNVX | vk::AccessFlagBits::eAccelerationStructureReadNVX };

			// Build a bottom-level acceleration structure for the new geometry
			// TODO: is this correct? Should we build a single BLAS for all geometries?
			command_buffer.buildAccelerationStructureNVX(
				bottom_levels.back().type,
				0,
				{},
				0,
				geometries.back(),
				{},
				false,
				bottom_levels.back().inner.get(),
				{},
				scratch_buffer.inner.get(),
				0,
				dispatch_loader);

			command_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eRaytracingNVX, vk::PipelineStageFlagBits::eRaytracingNVX, {}, memory_barrier, {}, {});

			// Build top-level acceleration structure - every instance has a transform (4x3 matrix), so we can use
			// the size of this vector as the total number of instances in the function call below
			command_buffer.buildAccelerationStructureNVX(
				top_level.type,
				static_cast<uint32_t>(transforms.size()),
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

	void remove_transform(size_t index)
	{
		// Remove the BLAS at `index`
	}

	void update_transform(size_t index)
	{
		// Update the BLAS at `index`
	}

	std::vector<vk::GeometryNVX> geometries;
	std::vector<Buffer> vertex_buffers;
	std::vector<Buffer> normal_buffers;
	std::vector<Buffer> index_buffers;
	std::vector<Buffer> primitive_buffers;
	std::vector<AccelerationStructure> bottom_levels;

	static const size_t max_instances = 256;
	size_t number_of_unique_geometries;

	// Updated every time a new BLAS is added (max)
	size_t scratch_memory_size = 0; 

	// CPU buffers
	std::vector<glm::mat4> transforms; 

	// GPU buffers
	Buffer instances_buffer; 
	Buffer scratch_buffer;

	// The top-level acceleration structure (TLAS)
	AccelerationStructure top_level;
};