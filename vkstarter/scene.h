#pragma once

#include "utilities.h"

class Scene
{
public:

	const AccelerationStructure& get_tlas() const { return top_level; }
	const std::vector<Buffer>& get_vertex_buffers() const { return vertex_buffers; }
	const std::vector<Buffer>& get_normal_buffers() const { return normal_buffers; }
	const std::vector<Buffer>& get_index_buffers() const { return index_buffers; }
	size_t get_number_of_instances() const { return transforms.size(); }

	void initialize(size_t capacity = max_instances)
	{
		// Create a buffer to hold the instance data
		size_t instance_buffer_size = sizeof(VkGeometryInstance) * capacity;
		instances_buffer = create_buffer(instance_buffer_size, vk::BufferUsageFlagBits::eRaytracingNVX, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
	}

	void add_geometry(const GeometryDefinition& geometry_def, const glm::mat4x3& transform = get_identity_matrix())
	{
		// Describe how the memory associated with these buffers will be accessed
		const auto memory_properties = vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible;

		// Describe the intended usage of these buffers
		const auto vertex_buffer_usage = vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eRaytracingNVX;
		const auto index_buffer_usage = vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eRaytracingNVX;

		// Create the buffers (and device memory)
		Buffer vertex_buffer = create_buffer(sizeof(geometry_def.vertices[0]) * geometry_def.vertices.size(), vertex_buffer_usage, memory_properties);
		Buffer index_buffer = create_buffer(sizeof(geometry_def.indices[0]) * geometry_def.indices.size(), index_buffer_usage, memory_properties);
		LOG_DEBUG("Created vertex and index buffers");

		upload(vertex_buffer, geometry_def.vertices);
		upload(index_buffer, geometry_def.indices);
		LOG_DEBUG("Uploaded vertex and index data to buffers");

		// Push back vertex buffers
		vertex_buffers.push_back(std::move(vertex_buffer));
		index_buffers.push_back(std::move(index_buffer));
		// TODO: normals...

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
		add_instance(transform);
	}

private:

	void add_instance(const glm::mat4x3& transform)
	{
		if (geometries.size() < max_instances)
		{
			size_t instance_id = geometries.size() - 1;

			// Describe the instance and its transform
			VkGeometryInstance instance;
			std::memcpy(instance.transform, glm::value_ptr(transform), sizeof(glm::mat4x3));
			instance.instanceId = instance_id; // Starts at 0 and increments...
			instance.mask = 0xff;
			instance.instanceOffset = 0;
			instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_CULL_DISABLE_BIT_NVX;
			instance.accelerationStructureHandle = bottom_levels.back().handle;

			const std::vector<VkGeometryInstance> instances = { instance };

			// Upload the transform data to the GPU
			const size_t offset = sizeof(VkGeometryInstance) * instance_id;
			upload(instances_buffer, instances, offset);

			// Keep track of the transform data on the CPU, as it may change and need to be
			// re-uploaded to the GPU
			transforms.push_back(transform);

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
	std::vector<AccelerationStructure> bottom_levels;

	static const size_t max_instances = 256;

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