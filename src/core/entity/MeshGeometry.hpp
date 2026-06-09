#pragma once

#include <cstdint>
#include <vector>

#include <Eigen/Core>

#include "Color.hpp"
#include "EntityId.hpp"

namespace cadino::core {

struct MeshGeometry {
    EntityId id{};
    EntityId group_id{};
    std::vector<Eigen::Vector3f> positions;
    std::vector<Eigen::Vector3f> normals;
    std::vector<std::uint32_t> indices;
    Color color{0.70f, 0.55f, 0.40f};
    float roughness{0.6f};
    float metallic{0.0f};
};

}  // namespace cadino::core
