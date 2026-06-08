#include "command/WallCommands.hpp"

#include "document/Document.hpp"

namespace cadino::core {

void AddWallCommand::apply(Document& doc) {
    if (!wall_.id.valid()) {
        wall_.id = next_entity_id();
    }
    doc.add_wall(wall_);
}

void AddWallCommand::undo(Document& doc) {
    doc.remove_wall(wall_.id);
}

void RemoveWallCommand::apply(Document& doc) {
    if (!captured_) {
        if (const auto* w = doc.find_wall(id_)) {
            snapshot_ = *w;
            captured_ = true;
        }
    }
    doc.remove_wall(id_);
}

void RemoveWallCommand::undo(Document& doc) {
    if (captured_) {
        doc.add_wall(snapshot_);
    }
}

void ModifyWallCommand::apply(Document& doc) {
    auto* w = doc.find_wall(id_);
    if (!w) {
        return;
    }
    if (!captured_) {
        before_ = *w;
        captured_ = true;
    }
    after_.id = id_;
    *w = after_;
}

void ModifyWallCommand::undo(Document& doc) {
    if (!captured_) {
        return;
    }
    auto* w = doc.find_wall(id_);
    if (!w) {
        return;
    }
    *w = before_;
}

}  // namespace cadino::core
