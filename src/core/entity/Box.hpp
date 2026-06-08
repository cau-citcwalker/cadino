#pragma once

#include <Eigen/Core>

#include "EntityId.hpp"

namespace cadino::core {

struct Box {
    EntityId id{};
    Eigen::Vector2d position{0.0, 0.0};
    Eigen::Vector2d size_xy{600.0, 600.0};
    double height{750.0};
    double base_z{0.0};
};

}  // namespace cadino::core
