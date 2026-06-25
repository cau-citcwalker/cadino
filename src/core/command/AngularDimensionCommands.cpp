#include "command/AngularDimensionCommands.hpp"

#include "document/Document.hpp"

namespace cadino::core {

void AddAngularDimensionCommand::apply(Document& doc) {
    if (!a_.id.valid()) a_.id = next_entity_id();
    doc.add_angular_dim(a_);
}
void AddAngularDimensionCommand::undo(Document& doc) { doc.remove_angular_dim(a_.id); }

void RemoveAngularDimensionCommand::apply(Document& doc) {
    if (!captured_) {
        if (const auto* a = doc.find_angular_dim(id_)) { snapshot_ = *a; captured_ = true; }
    }
    doc.remove_angular_dim(id_);
}
void RemoveAngularDimensionCommand::undo(Document& doc) {
    if (captured_) doc.add_angular_dim(snapshot_);
}

}  // namespace cadino::core
