#include "command/RadialDimensionCommands.hpp"

#include "document/Document.hpp"

namespace cadino::core {

void AddRadialDimensionCommand::apply(Document& doc) {
    if (!r_.id.valid()) r_.id = next_entity_id();
    doc.add_radial_dim(r_);
}
void AddRadialDimensionCommand::undo(Document& doc) { doc.remove_radial_dim(r_.id); }

void RemoveRadialDimensionCommand::apply(Document& doc) {
    if (!captured_) {
        if (const auto* r = doc.find_radial_dim(id_)) { snapshot_ = *r; captured_ = true; }
    }
    doc.remove_radial_dim(id_);
}
void RemoveRadialDimensionCommand::undo(Document& doc) {
    if (captured_) doc.add_radial_dim(snapshot_);
}

}  // namespace cadino::core
