#pragma once

#include <vector>

#include <Eigen/Core>

#include "Color.hpp"
#include "EntityId.hpp"

namespace cadino::core {

struct Slab {
    EntityId id{};
    EntityId layer_id{};
    std::vector<Eigen::Vector2d> outline;
    double level{0.0};
    double thickness{200.0};
    // Plan-view hatch:
    // 0 = none, 1 = solid, 2 = horiz, 3 = vert, 4 = cross,
    // 5 = diag /, 6 = diag \, 7 = diag cross.
    int hatch_pattern{0};
    Color hatch_color{0.75f, 0.65f, 0.55f};
    double hatch_scale{1.0};
};

}  // namespace cadino::core
