#include "command/BoxCommands.hpp"

#include "document/Document.hpp"

namespace cadino::core {

void AddBoxCommand::apply(Document& doc) {
    if (!box_.id.valid()) {
        box_.id = next_entity_id();
    }
    doc.add_box(box_);
}

void AddBoxCommand::undo(Document& doc) {
    doc.remove_box(box_.id);
}

void RemoveBoxCommand::apply(Document& doc) {
    if (!captured_) {
        if (const auto* b = doc.find_box(id_)) {
            snapshot_ = *b;
            captured_ = true;
        }
    }
    doc.remove_box(id_);
}

void RemoveBoxCommand::undo(Document& doc) {
    if (captured_) {
        doc.add_box(snapshot_);
    }
}

void ModifyBoxCommand::apply(Document& doc) {
    auto* b = doc.find_box(id_);
    if (!b) return;
    if (!captured_) {
        before_ = *b;
        captured_ = true;
    }
    after_.id = id_;
    *b = after_;
}

void ModifyBoxCommand::undo(Document& doc) {
    if (!captured_) return;
    auto* b = doc.find_box(id_);
    if (!b) return;
    *b = before_;
}

}  // namespace cadino::core
