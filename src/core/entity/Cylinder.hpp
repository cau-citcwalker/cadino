#pragma once

#include <Eigen/Core>

#include "EntityId.hpp"

namespace cadino::core {

struct Cylinder {
    EntityId id{};
    Eigen::Vector2d position{0.0, 0.0};
    double radius{300.0};
    double height{750.0};
    double base_z{0.0};
};

}  // namespace cadino::core
