#pragma once

#include <string>

#include <Eigen/Core>

#include "Color.hpp"
#include "EntityId.hpp"

namespace cadino::core {

struct Cylinder {
    EntityId id{};
    EntityId group_id{};
    EntityId layer_id{};
    Eigen::Vector2d position{0.0, 0.0};
    double radius{300.0};
    double height{750.0};
    double base_z{0.0};
    double rotation_x{0.0};  // radians around world X axis
    double rotation_y{0.0};  // radians around world Y axis
    Color color{0.55f, 0.70f, 0.82f};
    float roughness{0.5f};
    float metallic{0.0f};
    int pattern{0};
    std::string texture_path;
};

}  // namespace cadino::core
