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
EntityId Document::add_box(Box box)        { return insert_with_id(boxes_,   std::move(box));  }
EntityId Document::add_cylinder(Cylinder c){ return insert_with_id(cylinders_, std::move(c));  }
EntityId Document::add_mesh(MeshGeometry m){ return insert_with_id(meshes_,   std::move(m));  }
EntityId Document::add_curve(NurbsCurve c){ return insert_with_id(curves_,    std::move(c));  }

const Wall*   Document::find_wall(EntityId id)   const { return find_in(walls_,   id); }
Wall*         Document::find_wall(EntityId id)         { return find_in(walls_,   id); }
const Door*   Document::find_door(EntityId id)   const { return find_in(doors_,   id); }
Door*         Document::find_door(EntityId id)         { return find_in(doors_,   id); }
const Window* Document::find_window(EntityId id) const { return find_in(windows_, id); }
Window*       Document::find_window(EntityId id)       { return find_in(windows_, id); }
const Slab*   Document::find_slab(EntityId id)   const { return find_in(slabs_,   id); }
Slab*         Document::find_slab(EntityId id)         { return find_in(slabs_,   id); }
const Box*      Document::find_box(EntityId id)      const { return find_in(boxes_,     id); }
Box*            Document::find_box(EntityId id)            { return find_in(boxes_,     id); }
const Cylinder*     Document::find_cylinder(EntityId id) const { return find_in(cylinders_, id); }
Cylinder*           Document::find_cylinder(EntityId id)       { return find_in(cylinders_, id); }
const MeshGeometry* Document::find_mesh(EntityId id)     const { return find_in(meshes_, id); }
MeshGeometry*       Document::find_mesh(EntityId id)           { return find_in(meshes_, id); }
const NurbsCurve*   Document::find_curve(EntityId id)    const { return find_in(curves_, id); }
NurbsCurve*         Document::find_curve(EntityId id)          { return find_in(curves_, id); }

bool Document::remove_wall(EntityId id)   { return walls_.erase(id)   > 0; }
bool Document::remove_door(EntityId id)   { return doors_.erase(id)   > 0; }
bool Document::remove_window(EntityId id) { return windows_.erase(id) > 0; }
bool Document::remove_slab(EntityId id)   { return slabs_.erase(id)   > 0; }
bool Document::remove_box(EntityId id)      { return boxes_.erase(id)     > 0; }
bool Document::remove_cylinder(EntityId id) { return cylinders_.erase(id) > 0; }
bool Document::remove_mesh(EntityId id)     { return meshes_.erase(id)     > 0; }
bool Document::remove_curve(EntityId id)    { return curves_.erase(id)    > 0; }

}  // namespace cadino::core
