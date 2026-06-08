#pragma once

#include <unordered_map>

#include "entity/Box.hpp"
#include "entity/Door.hpp"
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

    bool remove_wall(EntityId id);
    bool remove_door(EntityId id);
    bool remove_window(EntityId id);
    bool remove_slab(EntityId id);
    bool remove_box(EntityId id);

    [[nodiscard]] const std::unordered_map<EntityId, Wall>& walls() const noexcept { return walls_; }
    [[nodiscard]] const std::unordered_map<EntityId, Door>& doors() const noexcept { return doors_; }
    [[nodiscard]] const std::unordered_map<EntityId, Window>& windows() const noexcept { return windows_; }
    [[nodiscard]] const std::unordered_map<EntityId, Slab>& slabs() const noexcept { return slabs_; }
    [[nodiscard]] const std::unordered_map<EntityId, Box>& boxes() const noexcept { return boxes_; }

    [[nodiscard]] std::size_t entity_count() const noexcept {
        return walls_.size() + doors_.size() + windows_.size() + slabs_.size() + boxes_.size();
    }

private:
    std::unordered_map<EntityId, Wall> walls_;
    std::unordered_map<EntityId, Door> doors_;
    std::unordered_map<EntityId, Window> windows_;
    std::unordered_map<EntityId, Slab> slabs_;
    std::unordered_map<EntityId, Box> boxes_;
};

}  // namespace cadino::core
