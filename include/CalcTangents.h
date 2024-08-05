#pragma once

#include <mikktspace.h>
#include <vector>
#include "VulkanTypes.h"

struct CalcTangentData {
    const std::vector<uint32_t>& indices;
    std::vector<Vertex>& vertices;
    const MeshPrimitive& primitive;
};

class CalcTangents {
public:
    CalcTangents();

    void calculate(CalcTangentData* data);

private:
    SMikkTSpaceInterface m_interface = {};
    SMikkTSpaceContext m_context = {};

    static int getVertexIndex(const SMikkTSpaceContext *pContext, int iFace, int iVert);

    static int getNumFaces(const SMikkTSpaceContext *pContext);

    static int getNumVerticesOfFace(const SMikkTSpaceContext *pContext, const int iFace);

    static void getPosition(const SMikkTSpaceContext *pContext, float fvPosOut[], const int iFace, const int iVert);

    static void getNormal(const SMikkTSpaceContext *pContext, float fvNormOut[], const int iFace, const int iVert);

    static void getTexCoord(const SMikkTSpaceContext *pContext, float fvTexcOut[], const int iFace, const int iVert);

    static void setTSpaceBasic(const SMikkTSpaceContext *pContext, const float fvTangent[], const float fSign,
                               const int iFace, const int iVert);
};