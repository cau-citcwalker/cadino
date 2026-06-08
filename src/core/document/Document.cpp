#include "document/Document.hpp"

namespace cadino::core {

namespace {

template <typename Map, typename Entity>
EntityId insert_with_id(Map& map, Entity entity) {
    if (!entity.id.valid()) {
        entity.id = next_entity_id();
    }
    const auto id = entity.id;
    map.emplace(id, std::move(entity));
    return id;
}

template <typename Map>
auto* find_in(Map& map, EntityId id) {
    const auto it = map.find(id);
    return it == map.end() ? nullptr : &it->second;
}

}  // namespace

EntityId Document::add_wall(Wall wall)     { return insert_with_id(walls_,   std::move(wall)); }
EntityId Document::add_door(Door door)     { return insert_with_id(doors_,   std::move(door)); }
EntityId Document::add_window(Window win)  { return insert_with_id(windows_, std::move(win));  }
EntityId Document::add_slab(Slab slab)     { return insert_with_id(slabs_,   std::move(slab)); }

const Wall*   Document::find_wall(EntityId id)   const { return find_in(walls_,   id); }
Wall*         Document::find_wall(EntityId id)         { return find_in(walls_,   id); }
const Door*   Document::find_door(EntityId id)   const { return find_in(doors_,   id); }
Door*         Document::find_door(EntityId id)         { return find_in(doors_,   id); }
const Window* Document::find_window(EntityId id) const { return find_in(windows_, id); }
Window*       Document::find_window(EntityId id)       { return find_in(windows_, id); }
const Slab*   Document::find_slab(EntityId id)   const { return find_in(slabs_,   id); }
Slab*         Document::find_slab(EntityId id)         { return find_in(slabs_,   id); }

bool Document::remove_wall(EntityId id)   { return walls_.erase(id)   > 0; }
bool Document::remove_door(EntityId id)   { return doors_.erase(id)   > 0; }
bool Document::remove_window(EntityId id) { return windows_.erase(id) > 0; }
bool Document::remove_slab(EntityId id)   { return slabs_.erase(id)   > 0; }

}  // namespace cadino::core
