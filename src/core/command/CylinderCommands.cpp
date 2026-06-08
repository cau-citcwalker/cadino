#include "command/CylinderCommands.hpp"

#include "document/Document.hpp"

namespace cadino::core {

void AddCylinderCommand::apply(Document& doc) {
    if (!cyl_.id.valid()) {
        cyl_.id = next_entity_id();
    }
    doc.add_cylinder(cyl_);
}

void AddCylinderCommand::undo(Document& doc) {
    doc.remove_cylinder(cyl_.id);
}

void RemoveCylinderCommand::apply(Document& doc) {
    if (!captured_) {
        if (const auto* c = doc.find_cylinder(id_)) {
            snapshot_ = *c;
            captured_ = true;
        }
    }
    doc.remove_cylinder(id_);
}

void RemoveCylinderCommand::undo(Document& doc) {
    if (captured_) {
        doc.add_cylinder(snapshot_);
    }
}

void ModifyCylinderCommand::apply(Document& doc) {
    auto* c = doc.find_cylinder(id_);
    if (!c) return;
    if (!captured_) {
        before_ = *c;
        captured_ = true;
    }
    after_.id = id_;
    *c = after_;
}

void ModifyCylinderCommand::undo(Document& doc) {
    if (!captured_) return;
    auto* c = doc.find_cylinder(id_);
    if (!c) return;
    *c = before_;
}

}  // namespace cadino::core
