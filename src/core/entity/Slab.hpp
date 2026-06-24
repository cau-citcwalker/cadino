#pragma once

#include <vector>

#include <Eigen/Core>

#include "EntityId.hpp"

namespace cadino::core {

struct Slab {
    EntityId id{};
    EntityId layer_id{};
    std::vector<Eigen::Vector2d> outline;
    double level{0.0};
    double thickness{200.0};
};

}  // namespace cadino::core
