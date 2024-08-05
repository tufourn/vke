#include "CalcTangents.h"

CalcTangents::CalcTangents() {
    m_interface.m_getNumFaces = getNumFaces;
    m_interface.m_getNumVerticesOfFace = getNumVerticesOfFace;
    m_interface.m_getPosition = getPosition;
    m_interface.m_getNormal = getNormal;
    m_interface.m_getTexCoord = getTexCoord;
    m_interface.m_setTSpaceBasic = setTSpaceBasic;

    m_context.m_pInterface = &m_interface;
}

int CalcTangents::getVertexIndex(const SMikkTSpaceContext *pContext, int iFace, int iVert) {
    const auto *data = static_cast<const CalcTangentData *>(pContext->m_pUserData);

    auto faceSize = getNumVerticesOfFace(pContext, iFace);
    auto indicesIndex = (iFace * faceSize) + iVert + data->primitive.indexStart;

    return static_cast<int>(data->indices[indicesIndex]);
}

int CalcTangents::getNumFaces(const SMikkTSpaceContext *pContext) {
    const auto *data = static_cast<const CalcTangentData *>(pContext->m_pUserData);

    return static_cast<int>(data->primitive.indexCount / 3);
}

int CalcTangents::getNumVerticesOfFace(const SMikkTSpaceContext *pContext, const int iFace) {
    return 3; // only supports triangle
}

void CalcTangents::getPosition(const SMikkTSpaceContext *pContext, float *fvPosOut, const int iFace, const int iVert) {
    const auto *data = static_cast<const CalcTangentData *>(pContext->m_pUserData);

    auto index = getVertexIndex(pContext, iFace, iVert);
    auto vertex = data->vertices[index];

    fvPosOut[0] = vertex.position.x;
    fvPosOut[1] = vertex.position.y;
    fvPosOut[2] = vertex.position.z;
}

void CalcTangents::getNormal(const SMikkTSpaceContext *pContext, float *fvNormOut, const int iFace, const int iVert) {
    const auto *data = static_cast<const CalcTangentData *>(pContext->m_pUserData);

    auto index = getVertexIndex(pContext, iFace, iVert);
    auto vertex = data->vertices[index];

    fvNormOut[0] = vertex.normal.x;
    fvNormOut[1] = vertex.normal.y;
    fvNormOut[2] = vertex.normal.z;
}

void CalcTangents::getTexCoord(const SMikkTSpaceContext *pContext, float *fvTexcOut, const int iFace, const int iVert) {
    const auto *data = static_cast<const CalcTangentData *>(pContext->m_pUserData);

    auto index = getVertexIndex(pContext, iFace, iVert);
    auto vertex = data->vertices[index];

    fvTexcOut[0] = vertex.uv_x;
    fvTexcOut[1] = vertex.uv_y;
}

void CalcTangents::setTSpaceBasic(const SMikkTSpaceContext *pContext, const float *fvTangent, const float fSign,
                                  const int iFace, const int iVert) {
    const auto *data = static_cast<const CalcTangentData *>(pContext->m_pUserData);
    auto index = getVertexIndex(pContext, iFace, iVert);
    auto *vertex = &data->vertices[index];

    vertex->tangent.x = fvTangent[0];
    vertex->tangent.y = fvTangent[1];
    vertex->tangent.z = fvTangent[2];
    vertex->tangent.w = fSign;
}

void CalcTangents::calculate(CalcTangentData *data) {
    m_context.m_pUserData = data;

    genTangSpaceDefault(&this->m_context);
}
