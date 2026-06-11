#pragma once

#include <unordered_map>

#include "entity/Block.hpp"
#include "entity/Box.hpp"
#include "entity/Cylinder.hpp"
#include "entity/Door.hpp"
#include "entity/MeshGeometry.hpp"
#include "entity/NurbsCurve.hpp"
#include "entity/Slab.hpp"
#include "entity/Wall.hpp"
#include "entity/Window.hpp"

namespace cadino::core {

class Document {
public:
    EntityId add_wall(Wall wall);
    EntityId add_door(Door door);
    EntityId add_window(Window win);
    EntityId add_slab(Slab slab);
    EntityId add_box(Box box);
    EntityId add_cylinder(Cylinder cyl);
    EntityId add_mesh(MeshGeometry mesh);
    EntityId add_curve(NurbsCurve curve);
    EntityId add_block(Block block);

    [[nodiscard]] const Wall* find_wall(EntityId id) const;
    [[nodiscard]] Wall* find_wall(EntityId id);
    [[nodiscard]] const Door* find_door(EntityId id) const;
    [[nodiscard]] Door* find_door(EntityId id);
    [[nodiscard]] const Window* find_window(EntityId id) const;
    [[nodiscard]] Window* find_window(EntityId id);
    [[nodiscard]] const Slab* find_slab(EntityId id) const;
    [[nodiscard]] Slab* find_slab(EntityId id);
    [[nodiscard]] const Box* find_box(EntityId id) const;
    [[nodiscard]] Box* find_box(EntityId id);
    [[nodiscard]] const Cylinder* find_cylinder(EntityId id) const;
    [[nodiscard]] Cylinder* find_cylinder(EntityId id);
    [[nodiscard]] const MeshGeometry* find_mesh(EntityId id) const;
    [[nodiscard]] MeshGeometry* find_mesh(EntityId id);
    [[nodiscard]] const NurbsCurve* find_curve(EntityId id) const;
    [[nodiscard]] NurbsCurve* find_curve(EntityId id);
    [[nodiscard]] const Block* find_block(EntityId id) const;
    [[nodiscard]] Block* find_block(EntityId id);

    bool remove_wall(EntityId id);
    bool remove_door(EntityId id);
    bool remove_window(EntityId id);
    bool remove_slab(EntityId id);
    bool remove_box(EntityId id);
    bool remove_cylinder(EntityId id);
    bool remove_mesh(EntityId id);
    bool remove_curve(EntityId id);
    bool remove_block(EntityId id);

    [[nodiscard]] const std::unordered_map<EntityId, Wall>& walls() const noexcept { return walls_; }
    [[nodiscard]] const std::unordered_map<EntityId, Door>& doors() const noexcept { return doors_; }
    [[nodiscard]] const std::unordered_map<EntityId, Window>& windows() const noexcept { return windows_; }
    [[nodiscard]] const std::unordered_map<EntityId, Slab>& slabs() const noexcept { return slabs_; }
    [[nodiscard]] const std::unordered_map<EntityId, Box>& boxes() const noexcept { return boxes_; }
    [[nodiscard]] const std::unordered_map<EntityId, Cylinder>& cylinders() const noexcept { return cylinders_; }
    [[nodiscard]] const std::unordered_map<EntityId, MeshGeometry>& meshes() const noexcept { return meshes_; }
    [[nodiscard]] const std::unordered_map<EntityId, NurbsCurve>& curves() const noexcept { return curves_; }
    [[nodiscard]] const std::unordered_map<EntityId, Block>& blocks() const noexcept { return blocks_; }

    [[nodiscard]] std::size_t entity_count() const noexcept {
        return walls_.size() + doors_.size() + windows_.size() + slabs_.size() +
               boxes_.size() + cylinders_.size() + meshes_.size() + curves_.size() + blocks_.size();
    }

private:
    std::unordered_map<EntityId, Wall> walls_;
    std::unordered_map<EntityId, Door> doors_;
    std::unordered_map<EntityId, Window> windows_;
    std::unordered_map<EntityId, Slab> slabs_;
    std::unordered_map<EntityId, Box> boxes_;
    std::unordered_map<EntityId, Cylinder> cylinders_;
    std::unordered_map<EntityId, MeshGeometry> meshes_;
    std::unordered_map<EntityId, NurbsCurve> curves_;
    std::unordered_map<EntityId, Block> blocks_;
};

}  // namespace cadino::core
