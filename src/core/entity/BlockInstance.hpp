#pragma once

#include <cmath>

#include <Eigen/Core>

#include "Box.hpp"
#include "Cylinder.hpp"
#include "EntityId.hpp"

namespace cadino::core {

// Reference to a BlockDefinition together with a 2D placement. Each instance
// renders the definition's children transformed by its own frame, so editing
// the definition propagates across every instance.
struct BlockInstance {
    EntityId id{};
    EntityId group_id{};
    EntityId definition_id{};
    Eigen::Vector2d position{0.0, 0.0};
    double rotation_z{0.0};
    double base_z{0.0};

    [[nodiscard]] Box world_box(const Box& local) const noexcept {
        const double c = std::cos(rotation_z);
        const double s = std::sin(rotation_z);
        Box out = local;
        out.position = position + Eigen::Vector2d{
            c * local.position.x() - s * local.position.y(),
            s * local.position.x() + c * local.position.y()};
        out.base_z = base_z + local.base_z;
        out.rotation_z = local.rotation_z + rotation_z;
        return out;
    }

    [[nodiscard]] Cylinder world_cylinder(const Cylinder& local) const noexcept {
        const double c = std::cos(rotation_z);
        const double s = std::sin(rotation_z);
        Cylinder out = local;
        out.position = position + Eigen::Vector2d{
            c * local.position.x() - s * local.position.y(),
            s * local.position.x() + c * local.position.y()};
        out.base_z = base_z + local.base_z;
        return out;
    }
};

}  // namespace cadino::core
