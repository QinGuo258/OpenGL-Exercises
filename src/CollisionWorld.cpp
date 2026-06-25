#include "CollisionWorld.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <unordered_set>
#include <cfloat>
#include <cmath>
#include <cstdio>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/intersect.hpp>

extern FILE* gLogFile;

#define LOG(fmt, ...) do { \
    if (gLogFile) { fprintf(gLogFile, fmt "\n", ##__VA_ARGS__); fflush(gLogFile); } \
} while(0)

// =============================================================================
// 2D 几何工具函数（XZ 平面，全部 static，仅在当前翻译单元可见）
// =============================================================================

/// 点到 2D 线段的最短距离
static float PointToSegmentDist2D(glm::vec2 p, glm::vec2 a, glm::vec2 b)
{
    glm::vec2 ab = b - a;
    float len2 = glm::dot(ab, ab);
    if (len2 < 1e-10f) return glm::length(p - a);
    float t = glm::dot(p - a, ab) / len2;
    t = glm::clamp(t, 0.0f, 1.0f);
    return glm::length(p - (a + t * ab));
}

/// 点到 2D 线段最近点
static glm::vec2 ClosestPointOnSegment2D(glm::vec2 p, glm::vec2 a, glm::vec2 b)
{
    glm::vec2 ab = b - a;
    float len2 = glm::dot(ab, ab);
    if (len2 < 1e-10f) return a;
    float t = glm::dot(p - a, ab) / len2;
    t = glm::clamp(t, 0.0f, 1.0f);
    return a + t * ab;
}

/// 二维三角形（XZ 投影）上距离点 p 最近的点
/// 使用重心坐标判断是否在三角形内部，否则取三条边上最近的点
glm::vec2 ClosestPointOnTriangle2D(glm::vec2 p, glm::vec2 a, glm::vec2 b, glm::vec2 c)
{
    glm::vec2 v0 = c - a;
    glm::vec2 v1 = b - a;
    glm::vec2 v2 = p - a;

    float d00 = glm::dot(v0, v0);
    float d01 = glm::dot(v0, v1);
    float d11 = glm::dot(v1, v1);
    float d20 = glm::dot(v2, v0);
    float d21 = glm::dot(v2, v1);

    float denom = d00 * d11 - d01 * d01;
    if (std::fabs(denom) > 1e-10f)
    {
        float v = (d11 * d20 - d01 * d21) / denom;
        float w = (d00 * d21 - d01 * d20) / denom;
        if (v >= -1e-5f && w >= -1e-5f && (v + w) <= 1.0f + 1e-5f)
        {
            // p 在三角形 XZ 投影内部 — 最近点即 p 自身
            return p;
        }
    }

    // 在外部 — 取三条边上最近的点
    glm::vec2 cpAB = ClosestPointOnSegment2D(p, a, b);
    glm::vec2 cpBC = ClosestPointOnSegment2D(p, b, c);
    glm::vec2 cpCA = ClosestPointOnSegment2D(p, c, a);

    float dAB2 = glm::dot(p - cpAB, p - cpAB);
    float dBC2 = glm::dot(p - cpBC, p - cpBC);
    float dCA2 = glm::dot(p - cpCA, p - cpCA);

    if (dAB2 <= dBC2 && dAB2 <= dCA2) return cpAB;
    if (dBC2 <= dAB2 && dBC2 <= dCA2) return cpBC;
    return cpCA;
}

/// 边函数法（Edge Function / Cross Product）测试点 (x,z) 是否在三角形 XZ 投影内
/// 使用 <= + 容差，防止站在两三角形缝隙边界上漏判
/// @return true 表示 (x,z) 落在三角形 XZ 投影内部或边缘上
static bool PointInTriangleXZ(float x, float z, const Triangle& tri)
{
    float px = x, pz = z;
    float ax = tri.v0.x, az = tri.v0.z;
    float bx = tri.v1.x, bz = tri.v1.z;
    float cx = tri.v2.x, cz = tri.v2.z;

    auto edge = [](float px, float pz, float e1x, float e1z, float e2x, float e2z) -> float {
        return (px - e1x) * (e2z - e1z) - (pz - e1z) * (e2x - e1x);
    };

    float e0 = edge(px, pz, ax, az, bx, bz); // 边 AB
    float e1 = edge(px, pz, bx, bz, cx, cz); // 边 BC
    float e2 = edge(px, pz, cx, cz, ax, az); // 边 CA

    // 允许 0.1mm 的浮点容差，使用 <= 而非 < 以覆盖边界线
    static constexpr float kEps = 1e-4f;

    // 三边同号（全 >= 或全 <=）即在三角形内或其边界上
    return (e0 >= -kEps && e1 >= -kEps && e2 >= -kEps)
        || (e0 <= kEps  && e1 <= kEps  && e2 <= kEps);
}

/// 平面方程法：由 N·(P - P0) = 0 求解三角形平面上的精确 Y 值
/// 要求 N.y != 0（调用侧保证已过滤 normal.y > 0.1）
/// 如果 (x,z) 不投影在三角形内，返回 -FLT_MAX
float TriangleHeightAtXZ(float x, float z, const Triangle& tri)
{
    if (!PointInTriangleXZ(x, z, tri))
        return -FLT_MAX;

    const glm::vec3& N = tri.normal;
    float ny = N.y;
    if (std::fabs(ny) < 1e-6f)
        return -FLT_MAX;

    // 平面方程：Nx*(x - P0.x) + Ny*(y - P0.y) + Nz*(z - P0.z) = 0
    // 解出 y = P0.y - (Nx*(x - P0.x) + Nz*(z - P0.z)) / Ny
    return tri.v0.y - (N.x * (x - tri.v0.x) + N.z * (z - tri.v0.z)) / ny;
}

// =============================================================================
// CollisionWorld 实现
// =============================================================================

CollisionWorld::CollisionWorld(const Model& model, const glm::mat4& modelMatrix, float cellSize)
    : m_CellSize(cellSize)
{
    LOG("=== CollisionWorld Construction ===");
    LOG("Cell size: %.2f", cellSize);

    // 法线变换矩阵（逆转置，支持非均匀缩放和旋转）
    glm::mat3 normalMatrix = glm::mat3(glm::transpose(glm::inverse(modelMatrix)));

    const auto& meshes = model.GetMeshes();
    LOG("Processing %zu meshes...", meshes.size());

    std::size_t totalTriangles = 0;

    for (std::size_t mi = 0; mi < meshes.size(); ++mi)
    {
        const auto& mesh = meshes[mi];

        // 跳过草丛/花朵(1) 和 水面(4) 的物理碰撞网格提取
        if (mesh.materialType == 1 || mesh.materialType == 4)
            continue;

        const auto& vertices = mesh.GetVertices();
        const auto& indices = mesh.GetIndices();

        if (indices.size() < 3) continue;

        std::size_t meshTriCount = indices.size() / 3;
        totalTriangles += meshTriCount;
        LOG("  Mesh[%zu]: %zu verts, %zu indices -> %zu triangles",
            mi, vertices.size(), indices.size(), meshTriCount);

        for (std::size_t i = 0; i + 2 < indices.size(); i += 3)
        {
            unsigned int i0 = indices[i];
            unsigned int i1 = indices[i + 1];
            unsigned int i2 = indices[i + 2];

            // 安全检查：索引越界则跳过
            if (i0 >= vertices.size() || i1 >= vertices.size() || i2 >= vertices.size())
                continue;

            glm::vec3 p0 = vertices[i0].Position;
            glm::vec3 p1 = vertices[i1].Position;
            glm::vec3 p2 = vertices[i2].Position;

            // 应用模型矩阵变换到世界空间
            glm::vec4 w0 = modelMatrix * glm::vec4(p0, 1.0f);
            glm::vec4 w1 = modelMatrix * glm::vec4(p1, 1.0f);
            glm::vec4 w2 = modelMatrix * glm::vec4(p2, 1.0f);

            Triangle tri;
            tri.v0 = glm::vec3(w0);
            tri.v1 = glm::vec3(w1);
            tri.v2 = glm::vec3(w2);

            // 计算世界空间法线
            glm::vec3 edge1 = tri.v1 - tri.v0;
            glm::vec3 edge2 = tri.v2 - tri.v0;
            glm::vec3 rawNormal = glm::cross(edge1, edge2);
            float normalLen = glm::length(rawNormal);
            if (normalLen < 1e-10f) continue; // 退化三角形
            tri.normal = glm::normalize(rawNormal);

            // 应用法线变换
            tri.normal = glm::normalize(normalMatrix * tri.normal);

            // 预计算包围盒
            tri.minX = std::min({ tri.v0.x, tri.v1.x, tri.v2.x });
            tri.maxX = std::max({ tri.v0.x, tri.v1.x, tri.v2.x });
            tri.minZ = std::min({ tri.v0.z, tri.v1.z, tri.v2.z });
            tri.maxZ = std::max({ tri.v0.z, tri.v1.z, tri.v2.z });
            tri.minY = std::min({ tri.v0.y, tri.v1.y, tri.v2.y });
            tri.maxY = std::max({ tri.v0.y, tri.v1.y, tri.v2.y });

            m_Triangles.push_back(tri);
        }
    }

    LOG("Total triangles extracted: %zu", m_Triangles.size());

    // 边界盒统计
    if (!m_Triangles.empty())
    {
        float wMinX = FLT_MAX, wMaxX = -FLT_MAX;
        float wMinY = FLT_MAX, wMaxY = -FLT_MAX;
        float wMinZ = FLT_MAX, wMaxZ = -FLT_MAX;
        for (const auto& tri : m_Triangles)
        {
            if (tri.minX < wMinX) wMinX = tri.minX;
            if (tri.maxX > wMaxX) wMaxX = tri.maxX;
            if (tri.minY < wMinY) wMinY = tri.minY;
            if (tri.maxY > wMaxY) wMaxY = tri.maxY;
            if (tri.minZ < wMinZ) wMinZ = tri.minZ;
            if (tri.maxZ > wMaxZ) wMaxZ = tri.maxZ;
        }
        LOG("World BBox X: [%.2f, %.2f]  Y: [%.2f, %.2f]  Z: [%.2f, %.2f]",
            wMinX, wMaxX, wMinY, wMaxY, wMinZ, wMaxZ);
    }

    BuildGrid(cellSize);
}

void CollisionWorld::BuildGrid(float cellSize)
{
    m_Grid.clear();

    for (int i = 0; i < static_cast<int>(m_Triangles.size()); ++i)
    {
        const Triangle& tri = m_Triangles[i];

        int cxMin = static_cast<int>(std::floor(tri.minX / cellSize));
        int cxMax = static_cast<int>(std::floor(tri.maxX / cellSize));
        int czMin = static_cast<int>(std::floor(tri.minZ / cellSize));
        int czMax = static_cast<int>(std::floor(tri.maxZ / cellSize));

        for (int cx = cxMin; cx <= cxMax; ++cx)
        {
            for (int cz = czMin; cz <= czMax; ++cz)
            {
                m_Grid[CellKey(cx, cz)].push_back(i);
            }
        }
    }

    LOG("Spatial hash grid: %zu cells (non-empty), avg %.1f triangles/cell",
        m_Grid.size(),
        m_Grid.empty() ? 0.0f : static_cast<float>(m_Triangles.size()) / m_Grid.size());
}

std::vector<const Triangle*> CollisionWorld::GetTrianglesNearXZ(float x, float z, float radius) const
{
    std::vector<const Triangle*> result;

    int cxMin = static_cast<int>(std::floor((x - radius) / m_CellSize));
    int cxMax = static_cast<int>(std::floor((x + radius) / m_CellSize));
    int czMin = static_cast<int>(std::floor((z - radius) / m_CellSize));
    int czMax = static_cast<int>(std::floor((z + radius) / m_CellSize));

    // 使用 unordered_set 对三角形索引去重
    // （用 vector + bool 数组在三角形数不极端时更高效，但 set 更通用）
    thread_local std::unordered_set<int> visited;
    visited.clear();

    for (int cx = cxMin; cx <= cxMax; ++cx)
    {
        for (int cz = czMin; cz <= czMax; ++cz)
        {
            auto it = m_Grid.find(CellKey(cx, cz));
            if (it == m_Grid.end()) continue;

            for (int triIdx : it->second)
            {
                if (visited.insert(triIdx).second)
                {
                    result.push_back(&m_Triangles[triIdx]);
                }
            }
        }
    }

    return result;
}

std::int64_t CollisionWorld::CellKey(int cx, int cz)
{
    return (static_cast<std::int64_t>(static_cast<std::uint64_t>(static_cast<std::uint32_t>(cx)) << 32)
            | static_cast<std::int64_t>(static_cast<std::uint32_t>(cz)));
}

std::vector<float> CollisionWorld::GetDebugLineVertices() const
{
    std::vector<float> result;
    result.reserve(m_Triangles.size() * 18); // 每条边 6 floats，每个三角形 3 条边

    for (const Triangle& tri : m_Triangles)
    {
        // 边 1: v0 → v1
        result.insert(result.end(), { tri.v0.x, tri.v0.y, tri.v0.z });
        result.insert(result.end(), { tri.v1.x, tri.v1.y, tri.v1.z });
        // 边 2: v1 → v2
        result.insert(result.end(), { tri.v1.x, tri.v1.y, tri.v1.z });
        result.insert(result.end(), { tri.v2.x, tri.v2.y, tri.v2.z });
        // 边 3: v2 → v0
        result.insert(result.end(), { tri.v2.x, tri.v2.y, tri.v2.z });
        result.insert(result.end(), { tri.v0.x, tri.v0.y, tri.v0.z });
    }

    return result;
}

bool CollisionWorld::Raycast(glm::vec3 start, glm::vec3 dir, float maxDistance, float& outHitDistance) const
{
    glm::vec3 end = start + dir * maxDistance;

    // 射线段的 XZ 包围盒
    float segMinX = std::min(start.x, end.x);
    float segMaxX = std::max(start.x, end.x);
    float segMinZ = std::min(start.z, end.z);
    float segMaxZ = std::max(start.z, end.z);

    int cxMin = static_cast<int>(std::floor(segMinX / m_CellSize));
    int cxMax = static_cast<int>(std::floor(segMaxX / m_CellSize));
    int czMin = static_cast<int>(std::floor(segMinZ / m_CellSize));
    int czMax = static_cast<int>(std::floor(segMaxZ / m_CellSize));

    float closestDist = maxDistance + 1.0f;
    bool hit = false;

    thread_local std::unordered_set<int> visited;
    visited.clear();

    for (int cx = cxMin; cx <= cxMax; ++cx)
    {
        for (int cz = czMin; cz <= czMax; ++cz)
        {
            auto it = m_Grid.find(CellKey(cx, cz));
            if (it == m_Grid.end()) continue;

            for (int triIdx : it->second)
            {
                if (!visited.insert(triIdx).second) continue;

                const Triangle& tri = m_Triangles[triIdx];

                glm::vec2 bary;
                float distance;

                if (glm::intersectRayTriangle(start, dir, tri.v0, tri.v1, tri.v2, bary, distance))
                {
                    if (distance > 0.0f && distance < closestDist)
                    {
                        closestDist = distance;
                        hit = true;
                    }
                }
            }
        }
    }

    if (hit)
        outHitDistance = closestDist;
    return hit;
}
