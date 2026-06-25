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

Document::Document() {
    Layer default_layer;
    default_layer.name = "Default";
    default_layer.color = Color{0.85f, 0.85f, 0.85f};
    default_layer_ = add_layer(std::move(default_layer));
    active_layer_ = default_layer_;
}

EntityId Document::add_layer(Layer layer) { return insert_with_id(layers_, std::move(layer)); }
const Layer* Document::find_layer(EntityId id) const { return find_in(layers_, id); }
Layer*       Document::find_layer(EntityId id)       { return find_in(layers_, id); }
bool Document::remove_layer(EntityId id) {
    if (id == default_layer_) return false;
    if (active_layer_ == id) active_layer_ = default_layer_;
    return layers_.erase(id) > 0;
}

EntityId Document::add_wall(Wall wall) {
    if (!wall.layer_id.valid()) wall.layer_id = active_layer_;
    return insert_with_id(walls_, std::move(wall));
}
EntityId Document::add_door(Door door)     { return insert_with_id(doors_,   std::move(door)); }
EntityId Document::add_window(Window win)  { return insert_with_id(windows_, std::move(win));  }
EntityId Document::add_slab(Slab slab) {
    if (!slab.layer_id.valid()) slab.layer_id = active_layer_;
    return insert_with_id(slabs_, std::move(slab));
}
EntityId Document::add_box(Box box) {
    if (!box.layer_id.valid()) box.layer_id = active_layer_;
    return insert_with_id(boxes_, std::move(box));
}
EntityId Document::add_cylinder(Cylinder c) {
    if (!c.layer_id.valid()) c.layer_id = active_layer_;
    return insert_with_id(cylinders_, std::move(c));
}
EntityId Document::add_mesh(MeshGeometry m){ return insert_with_id(meshes_,   std::move(m));  }
EntityId Document::add_curve(NurbsCurve c) {
    if (!c.layer_id.valid()) c.layer_id = active_layer_;
    return insert_with_id(curves_, std::move(c));
}
EntityId Document::add_block(Block b) {
    if (!b.layer_id.valid()) b.layer_id = active_layer_;
    return insert_with_id(blocks_, std::move(b));
}
EntityId Document::add_surface(NurbsSurface s) {
    if (!s.layer_id.valid()) s.layer_id = active_layer_;
    return insert_with_id(surfaces_, std::move(s));
}
EntityId Document::add_block_def(BlockDefinition d){ return insert_with_id(block_defs_, std::move(d));}
EntityId Document::add_block_instance(BlockInstance i) {
    if (!i.layer_id.valid()) i.layer_id = active_layer_;
    return insert_with_id(block_instances_, std::move(i));
}
EntityId Document::add_dimension(Dimension d) {
    if (!d.layer_id.valid()) d.layer_id = active_layer_;
    return insert_with_id(dimensions_, std::move(d));
}
EntityId Document::add_text(TextAnnotation t) {
    if (!t.layer_id.valid()) t.layer_id = active_layer_;
    return insert_with_id(texts_, std::move(t));
}
EntityId Document::add_leader(Leader l) {
    if (!l.layer_id.valid()) l.layer_id = active_layer_;
    return insert_with_id(leaders_, std::move(l));
}
EntityId Document::add_angular_dim(AngularDimension a) {
    if (!a.layer_id.valid()) a.layer_id = active_layer_;
    return insert_with_id(angular_dims_, std::move(a));
}
EntityId Document::add_radial_dim(RadialDimension r) {
    if (!r.layer_id.valid()) r.layer_id = active_layer_;
    return insert_with_id(radial_dims_, std::move(r));
}

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
const Block*        Document::find_block(EntityId id)    const { return find_in(blocks_, id); }
Block*              Document::find_block(EntityId id)          { return find_in(blocks_, id); }
const NurbsSurface* Document::find_surface(EntityId id)  const { return find_in(surfaces_, id); }
NurbsSurface*       Document::find_surface(EntityId id)        { return find_in(surfaces_, id); }
const BlockDefinition* Document::find_block_def(EntityId id) const { return find_in(block_defs_, id); }
BlockDefinition*       Document::find_block_def(EntityId id)       { return find_in(block_defs_, id); }
const BlockInstance*   Document::find_block_instance(EntityId id) const { return find_in(block_instances_, id); }
BlockInstance*         Document::find_block_instance(EntityId id)       { return find_in(block_instances_, id); }
const Dimension*       Document::find_dimension(EntityId id) const { return find_in(dimensions_, id); }
Dimension*             Document::find_dimension(EntityId id)       { return find_in(dimensions_, id); }
const TextAnnotation*  Document::find_text(EntityId id) const { return find_in(texts_, id); }
TextAnnotation*        Document::find_text(EntityId id)       { return find_in(texts_, id); }
const Leader*          Document::find_leader(EntityId id) const { return find_in(leaders_, id); }
Leader*                Document::find_leader(EntityId id)       { return find_in(leaders_, id); }
const AngularDimension* Document::find_angular_dim(EntityId id) const { return find_in(angular_dims_, id); }
AngularDimension*       Document::find_angular_dim(EntityId id)       { return find_in(angular_dims_, id); }
const RadialDimension*  Document::find_radial_dim(EntityId id) const { return find_in(radial_dims_, id); }
RadialDimension*        Document::find_radial_dim(EntityId id)       { return find_in(radial_dims_, id); }

bool Document::remove_wall(EntityId id)   { return walls_.erase(id)   > 0; }
bool Document::remove_door(EntityId id)   { return doors_.erase(id)   > 0; }
bool Document::remove_window(EntityId id) { return windows_.erase(id) > 0; }
bool Document::remove_slab(EntityId id)   { return slabs_.erase(id)   > 0; }
bool Document::remove_box(EntityId id)      { return boxes_.erase(id)     > 0; }
bool Document::remove_cylinder(EntityId id) { return cylinders_.erase(id) > 0; }
bool Document::remove_mesh(EntityId id)     { return meshes_.erase(id)     > 0; }
bool Document::remove_curve(EntityId id)    { return curves_.erase(id)    > 0; }
bool Document::remove_block(EntityId id)    { return blocks_.erase(id)    > 0; }
bool Document::remove_surface(EntityId id)  { return surfaces_.erase(id)  > 0; }
bool Document::remove_block_def(EntityId id){ return block_defs_.erase(id) > 0; }
bool Document::remove_block_instance(EntityId id){ return block_instances_.erase(id) > 0; }
bool Document::remove_dimension(EntityId id){ return dimensions_.erase(id) > 0; }
bool Document::remove_text(EntityId id){ return texts_.erase(id) > 0; }
bool Document::remove_leader(EntityId id){ return leaders_.erase(id) > 0; }
bool Document::remove_angular_dim(EntityId id){ return angular_dims_.erase(id) > 0; }
bool Document::remove_radial_dim(EntityId id){ return radial_dims_.erase(id) > 0; }

}  // namespace cadino::core
