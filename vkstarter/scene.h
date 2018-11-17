#pragma once

#include <glm.hpp>

#include "utilities.h"

class Scene
{
public:

	Scene();

	void build()
	{

	}

	const std::vector<glm::vec3>& get_vertices() const { return vertices; }
	const std::vector<uint32_t>& get_indices() const { return indices; }
	const glm::mat4& get_transform() const { return transform; }

private:

	std::vector<glm::vec3> vertices;
	std::vector<uint32_t> indices;

	glm::mat4 transform;

	Buffer vertex_buffer;
	Buffer index_buffer;
	Buffer instance_buffer;
	Buffer scratch_buffer;
	AccelerationStructure top_level;
	std::vector<AccelerationStructure> bottom_levels;
};