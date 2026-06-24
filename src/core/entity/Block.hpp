#pragma once

#include <string>
#include <vector>

#include <Eigen/Core>

#include "Box.hpp"
#include "Cylinder.hpp"
#include "EntityId.hpp"

namespace cadino::core {

// A Block is a reusable group of primitives. Its children are stored in a
// local frame; the block carries a 2D position, a rotation around Z, and a
// vertical offset to place that frame in world space.
struct Block {
    EntityId id{};
    EntityId group_id{};
    EntityId layer_id{};
    std::string name{"Block"};
    Eigen::Vector2d position{0.0, 0.0};
    double rotation_z{0.0};   // radians
    double base_z{0.0};
    std::vector<Box> boxes;          // child boxes in local coords
    std::vector<Cylinder> cylinders; // child cylinders in local coords

    [[nodiscard]] Box world_box(const Box& local) const noexcept {
        const double c = std::cos(rotation_z);
        const double s = std::sin(rotation_z);
        Box out = local;
        out.position = position + Eigen::Vector2d{c * local.position.x() - s * local.position.y(),
                                                  s * local.position.x() + c * local.position.y()};
        out.base_z = base_z + local.base_z;
        out.rotation_z = local.rotation_z + rotation_z;
        return out;
    }

    [[nodiscard]] Cylinder world_cylinder(const Cylinder& local) const noexcept {
        const double c = std::cos(rotation_z);
        const double s = std::sin(rotation_z);
        Cylinder out = local;
        out.position = position + Eigen::Vector2d{c * local.position.x() - s * local.position.y(),
                                                  s * local.position.x() + c * local.position.y()};
        out.base_z = base_z + local.base_z;
        return out;
    }
};

}  // namespace cadino::core
