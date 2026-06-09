#pragma once

#include <Eigen/Core>

#include "Color.hpp"
#include "EntityId.hpp"

namespace cadino::core {

struct Cylinder {
    EntityId id{};
    EntityId group_id{};
    Eigen::Vector2d position{0.0, 0.0};
    double radius{300.0};
    double height{750.0};
    double base_z{0.0};
    Color color{0.55f, 0.70f, 0.82f};
    float roughness{0.5f};
    float metallic{0.0f};
};

}  // namespace cadino::core
