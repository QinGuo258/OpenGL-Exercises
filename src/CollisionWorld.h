#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include "Model.h"

/// 世界空间中的三角形（已应用模型变换矩阵）
struct Triangle
{
    glm::vec3 v0, v1, v2;
    glm::vec3 normal;   // 单位法线（世界空间）
    float minY, maxY;   // Y 轴范围，用于快速剔除
    float minX, maxX, minZ, maxZ; // XZ 包围盒，用于空间哈希
};

/// 在二维三角形 XZ 投影上找到距离点 p 最近的点
glm::vec2 ClosestPointOnTriangle2D(glm::vec2 p, glm::vec2 a, glm::vec2 b, glm::vec2 c);

/// 重心坐标内插：给定世界空间 XZ 坐标，求三角形平面上的 Y 值
/// 如果 (x,z) 不投影在三角形内部，返回 -FLT_MAX
float TriangleHeightAtXZ(float x, float z, const Triangle& tri);

/// 基于 XZ 平面 2D 空间哈希网格的静态碰撞世界
class CollisionWorld
{
public:
    /// @param model      已加载的地图模型
    /// @param modelMatrix 与渲染时传入 uModel 完全一致的变换矩阵
    /// @param cellSize    空间哈希格子大小（XZ 平面），默认 5.0
    CollisionWorld(const Model& model, const glm::mat4& modelMatrix, float cellSize = 5.0f);

    /// 查询以 (x,z) 为圆心、radius 为半径范围内的三角形（指针，无拷贝）
    std::vector<const Triangle*> GetTrianglesNearXZ(float x, float z, float radius) const;

    const std::vector<Triangle>& GetTriangles() const { return m_Triangles; }

    /// 生成 Debug Draw 用线段顶点数据
    std::vector<float> GetDebugLineVertices() const;

    /// 射线与地图三角形相交检测（Möller–Trumbore 算法）
    /// @param start         射线起点（世界空间）
    /// @param dir           射线方向（必须已归一化）
    /// @param maxDistance   最大检测距离
    /// @param outHitDistance 命中距离（仅当返回 true 时有效）
    /// @return 是否命中
    bool Raycast(glm::vec3 start, glm::vec3 dir, float maxDistance, float& outHitDistance) const;

private:
    void BuildGrid(float cellSize);

    static std::int64_t CellKey(int cx, int cz);

    std::vector<Triangle> m_Triangles;
    std::unordered_map<std::int64_t, std::vector<int>> m_Grid;
    float m_CellSize = 5.0f;
};
