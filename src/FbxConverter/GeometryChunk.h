#pragma once

#include "FbxImporter.h"
#include "GeometryCommon.h"
#include "FileOutputStream.h"

#include <vector>

class CGeometryChunk
{
    static CGeometryChunk s_cInstance;

public:
    static CGeometryChunk& getInstance() { return s_cInstance; }

protected:
    CGeometryChunk()
    {
        m_ExportTriList = false;
    }
    ~CGeometryChunk() {}

public:
    bool export(
        uint32_t maxJointMtxNum,
        FileOutputStream* pOut,
        aten::FbxImporter* pImporter);

    void Clear();

    const izanagi::math::SVector4& GetMin() { return m_vMin; }
    const izanagi::math::SVector4& GetMax() { return m_vMax; }

    /** トライアングルリストで出力するかどうかを設定.
     */
    void setIsExportTriList(bool flag) { m_ExportTriList = flag; }

protected:
    bool exportGroup(
        FileOutputStream* pOut,
        aten::FbxImporter* pImporter);

    void bindJointToTriangle(
        aten::FbxImporter* pImporter,
        MeshInfo& sMesh);

    void classifyTriangleByJoint(MeshInfo& sMesh);

    void getMeshInfo(
        aten::FbxImporter* pImporter,
        MeshInfo& sMesh);

    bool computeVtxNormal(
        aten::FbxImporter* pImporter,
        const TriangleParam& sTri);

    bool computeVtxTangent(
        aten::FbxImporter* pImporter,
        const TriangleParam& sTri);

    void computeVtxParemters(aten::FbxImporter* pImporter);

    uint32_t exportVertices(
        FileOutputStream* pOut,
        aten::FbxImporter* pImporter);

    bool exportVertices(
        FileOutputStream* pOut,
        aten::FbxImporter* pImporter,
        const MeshInfo& sMesh,
        PrimitiveSetParam& sPrimSet);

    bool exportMesh(
        FileOutputStream* pOut,
        aten::FbxImporter* pImporter);

    void getMinMaxPos(
        aten::FbxImporter* pImporter,
        izanagi::math::SVector4& vMin,
        izanagi::math::SVector4& vMax,
        const PrimitiveSetParam& sPrimSet);

    bool exportPrimitiveSet(
        FileOutputStream* pOut,
        aten::FbxImporter* pImporter,
        const PrimitiveSetParam& sPrimSet);

    uint32_t exportIndices(
        FileOutputStream* pOut,
        aten::FbxImporter* pImporter,
        const PrimitiveSetParam& sPrimSet);

protected:
    std::vector<MeshInfo> m_MeshList;
    std::vector<TriangleParam> m_TriList;
    std::vector<SkinParam> m_SkinList;
    std::vector<VtxAdditional> m_VtxList;

    std::vector<uint32_t> m_ExportedVtx;

    izanagi::math::SVector4 m_vMin;
    izanagi::math::SVector4 m_vMax;

    izanagi::S_MSH_HEADER m_Header;

    // 最大ボーンマトリクス数
    uint32_t m_MaxJointMtxNum;

    // トライアングルリストで出力するかどうか
	bool m_ExportTriList{ true };
};
