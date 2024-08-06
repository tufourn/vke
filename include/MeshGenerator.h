#pragma once

#include <VulkanTypes.h>

struct MeshBuffers {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
};

MeshBuffers createCubeMesh(float edgeX, float edgeY, float edgeZ);
MeshBuffers createSphereMesh(float radius, uint32_t meridians, uint32_t parallels);

static void generateTangents(MeshBuffers& meshBuffers);