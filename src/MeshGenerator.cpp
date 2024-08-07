#include <numbers>
#include "MeshGenerator.h"
#include "CalcTangents.h"

void generateTangents(MeshBuffers &meshBuffers) {
    CalcTangents mikktspace;
    CalcTangentData tangentData = {
            meshBuffers.indices,
            meshBuffers.vertices,
            { // pass only relevant data to calc tangent
                    .indexStart = 0,
                    .indexCount = static_cast<uint32_t>(meshBuffers.indices.size()),
            }
    };
    mikktspace.calculate(&tangentData);
    for (auto& vertex : meshBuffers.vertices) {
        vertex.bitangent =
                glm::vec4(glm::cross(vertex.normal,
                                     glm::vec3(vertex.tangent.x, vertex.tangent.y, vertex.tangent.z)),
                          vertex.tangent.w);
    }
}

MeshBuffers createCubeMesh(float edgeX, float edgeY, float edgeZ) {
    MeshBuffers meshBuffers;

    float halfEdgeX = edgeX / 2.f;
    float halfEdgeY = edgeY / 2.f;
    float halfEdgeZ = edgeZ / 2.f;

//                3+-------------------+2
//                /|                  /|
//               / |                 / |
//              /  |                /  |
//             /   |               /   |
//            /    |              /    |
//          7+-------------------+6    |
//           |     |             |     |
//           |     |             |     |
//           |     |             |     |
//           |     |             |     |
//           |    0+-------------|-----+1
//           |    /              |    /
//           |   /               |   /
//           |  /                |  /
//           | /                 | /
//           |/                  |/
//          4+-------------------+5
//
//           +y
//           |
//           |
//           +------ +x
//          /
//         /
//        +z


    // Positions of the 8 vertices of the cuboid
    glm::vec3 positions[8] = {
            {-halfEdgeX, -halfEdgeY, -halfEdgeZ}, // 0
            {halfEdgeX,  -halfEdgeY, -halfEdgeZ},  // 1
            {halfEdgeX,  halfEdgeY,  -halfEdgeZ},  // 2
            {-halfEdgeX, halfEdgeY,  -halfEdgeZ}, // 3
            {-halfEdgeX, -halfEdgeY, halfEdgeZ}, // 4
            {halfEdgeX,  -halfEdgeY, halfEdgeZ},  // 5
            {halfEdgeX,  halfEdgeY,  halfEdgeZ},  // 6
            {-halfEdgeX, halfEdgeY,  halfEdgeZ}  // 7
    };

    // Normals for each face of the cuboid
    glm::vec3 normals[6] = {
            {0.0f,  0.0f,  -1.0f}, // Back face   0
            {0.0f,  0.0f,  1.0f},  // Front face  1
            {-1.0f, 0.0f,  0.0f}, // Left face   2
            {1.0f,  0.0f,  0.0f},  // Right face  3
            {0.0f,  -1.0f, 0.0f}, // Bottom face 4
            {0.0f,  1.0f,  0.0f}   // Top face    5
    };

    // UV coordinates for each face of the cuboid
    glm::vec2 uvs[4] = {
            {0.0f, 0.0f}, // Bottom left  0
            {1.0f, 0.0f}, // Bottom right 1
            {1.0f, 1.0f}, // Top right    2
            {0.0f, 1.0f}  // Top left     3
    };

    meshBuffers.vertices = {
            // back face
            {.position = positions[1], .uv_x = uvs[0].x, .normal = normals[0], .uv_y = uvs[0].y}, // 0
            {.position = positions[0], .uv_x = uvs[1].x, .normal = normals[0], .uv_y = uvs[1].y}, // 1
            {.position = positions[3], .uv_x = uvs[2].x, .normal = normals[0], .uv_y = uvs[2].y}, // 2
            {.position = positions[2], .uv_x = uvs[3].x, .normal = normals[0], .uv_y = uvs[3].y}, // 3

            // front face
            {.position = positions[4], .uv_x = uvs[0].x, .normal = normals[1], .uv_y = uvs[0].y}, // 4
            {.position = positions[5], .uv_x = uvs[1].x, .normal = normals[1], .uv_y = uvs[1].y}, // 5
            {.position = positions[6], .uv_x = uvs[2].x, .normal = normals[1], .uv_y = uvs[2].y}, // 6
            {.position = positions[7], .uv_x = uvs[3].x, .normal = normals[1], .uv_y = uvs[3].y}, // 7

            // left face
            {.position = positions[0], .uv_x = uvs[0].x, .normal = normals[2], .uv_y = uvs[0].y}, // 8
            {.position = positions[4], .uv_x = uvs[1].x, .normal = normals[2], .uv_y = uvs[1].y}, // 9
            {.position = positions[7], .uv_x = uvs[2].x, .normal = normals[2], .uv_y = uvs[2].y}, // 10
            {.position = positions[3], .uv_x = uvs[3].x, .normal = normals[2], .uv_y = uvs[3].y}, // 11

            // right face
            {.position = positions[5], .uv_x = uvs[0].x, .normal = normals[3], .uv_y = uvs[0].y}, // 12
            {.position = positions[1], .uv_x = uvs[1].x, .normal = normals[3], .uv_y = uvs[1].y}, // 13
            {.position = positions[2], .uv_x = uvs[2].x, .normal = normals[3], .uv_y = uvs[2].y}, // 14
            {.position = positions[6], .uv_x = uvs[3].x, .normal = normals[3], .uv_y = uvs[3].y}, // 15

            // bottom face
            {.position = positions[0], .uv_x = uvs[0].x, .normal = normals[4], .uv_y = uvs[0].y}, // 16
            {.position = positions[1], .uv_x = uvs[1].x, .normal = normals[4], .uv_y = uvs[1].y}, // 17
            {.position = positions[5], .uv_x = uvs[2].x, .normal = normals[4], .uv_y = uvs[2].y}, // 18
            {.position = positions[4], .uv_x = uvs[3].x, .normal = normals[4], .uv_y = uvs[3].y}, // 19

            // top face
            {.position = positions[7], .uv_x = uvs[0].x, .normal = normals[5], .uv_y = uvs[0].y}, // 20
            {.position = positions[6], .uv_x = uvs[1].x, .normal = normals[5], .uv_y = uvs[1].y}, // 21
            {.position = positions[2], .uv_x = uvs[2].x, .normal = normals[5], .uv_y = uvs[2].y}, // 22
            {.position = positions[3], .uv_x = uvs[3].x, .normal = normals[5], .uv_y = uvs[3].y}, // 23
    };

    // Indices, in counterclockwise winding
    meshBuffers.indices = {
            0, 1, 2, 0, 2, 3,       // back face
            4, 5, 6, 4, 6, 7,       // front face
            8, 9, 10, 8, 10, 11,    // left face
            12, 13, 14, 12, 14, 15, // right face
            16, 17, 18, 16, 18, 19, // bottom face
            20, 21, 22, 20, 22, 23, // top face
    };

    generateTangents(meshBuffers);

    return meshBuffers;
}

MeshBuffers createSphereMesh(float radius, uint32_t meridians, uint32_t parallels) {
    MeshBuffers meshBuffers = {};

    for (uint32_t p_i = 0; p_i <= parallels; p_i++) {
        float theta = p_i * std::numbers::pi / parallels;
        float sinTheta = sinf(theta);
        float cosTheta = cosf(theta);

        for (uint32_t m_i = 0; m_i <= meridians; m_i++) {
            float phi = m_i * 2 * std::numbers::pi / meridians;
            float sinPhi = sinf(phi);
            float cosPhi = cosf(phi);

            glm::vec3 position = glm::vec3(
                    radius * cosPhi * sinTheta,
                    radius * cosTheta,
                    radius * sinPhi * sinTheta
            );
            glm::vec3 normal = glm::normalize(position);
            glm::vec2 uv = glm::vec2(
                    static_cast<float>(m_i) / meridians,
                    static_cast<float>(p_i) / parallels
            );

            Vertex vertex = {
                    .position = position,
                    .uv_x = uv.x,
                    .normal = normal,
                    .uv_y = uv.y,
            };

            meshBuffers.vertices.emplace_back(vertex);
        }
    }

    for (uint32_t p_i = 0; p_i < parallels; ++p_i) {
        for (uint32_t m_i = 0; m_i < meridians; ++m_i) {
            uint32_t first = p_i * (meridians + 1) + m_i;
            uint32_t second = first + meridians + 1;

            meshBuffers.indices.push_back(first);
            meshBuffers.indices.push_back(second);
            meshBuffers.indices.push_back(first + 1);

            meshBuffers.indices.push_back(second);
            meshBuffers.indices.push_back(second + 1);
            meshBuffers.indices.push_back(first + 1);
        }
    }

    generateTangents(meshBuffers);

    return meshBuffers;

}
