#pragma once

#include <Eigen/Core>

#include "Color.hpp"
#include "EntityId.hpp"

namespace cadino::core {

struct Box {
    EntityId id{};
    EntityId group_id{};
    Eigen::Vector2d position{0.0, 0.0};
    Eigen::Vector2d size_xy{600.0, 600.0};
    double height{750.0};
    double base_z{0.0};
    double rotation_z{0.0};  // radians, around vertical axis at position
    Color color{0.78f, 0.62f, 0.40f};
};

}  // namespace cadino::core
