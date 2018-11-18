#pragma once

#include "utilities.h"

#include "gtc/type_ptr.hpp"

class GeometryInstance
{
public:

	GeometryInstance(const vk::Device& device)
	{

	}

	void build_accel()
	{
		// First, build the geometry
		auto geometry_triangles = vk::GeometryTrianglesNVX{}
			.setIndexCount(static_cast<uint32_t>(geometry_def.indices.size()))
			.setIndexData(index_buffer.buffer.get())
			.setIndexType(vk::IndexType::eUint32)
			.setVertexCount(static_cast<uint32_t>(geometry_def.vertices.size()))
			.setVertexData(vertex_buffer.buffer.get())
			.setVertexFormat(vk::Format::eR32G32B32Sfloat)
			.setVertexStride(sizeof(geometry_def.vertices[0]));

		auto geometry_data = vk::GeometryDataNVX{ geometry_triangles };
		geometry = vk::GeometryNVX{ vk::GeometryTypeNVX::eTriangles, geometry_data, vk::GeometryFlagBitsNVX::eOpaque };
	
		// Then, build the bottom-level acceleration structure for this geometry instance
		// ...
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

	Scene(const vk::Device& device, size_t capacity = max_instances)
	{
		// Create a buffer to hold the instance data
		size_t instance_buffer_size = sizeof(VkGeometryInstance) * capacity;
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

			// Upload the transform data to the GPU
			size_t offset = sizeof(instance) * number_of_instances;
			//instance_buffer = create_buffer(sizeof(VkGeometryInstance), vk::BufferUsageFlagBits::eRaytracingNVX, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
			//upload(instance_buffer, instances, offset);

			// Keep track of the transform data on the CPU, as well
			transforms.push_back(transform);

			// Keep track of how many instances we've allocated
			number_of_instances++;

			// Update the TLAS
			build_accel();
		}
	}

	void build_accel()
	{

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