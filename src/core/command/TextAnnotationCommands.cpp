#include "command/TextAnnotationCommands.hpp"

#include "document/Document.hpp"

namespace cadino::core {

void AddTextAnnotationCommand::apply(Document& doc) {
    if (!t_.id.valid()) t_.id = next_entity_id();
    doc.add_text(t_);
}

void AddTextAnnotationCommand::undo(Document& doc) {
    doc.remove_text(t_.id);
}

void RemoveTextAnnotationCommand::apply(Document& doc) {
    if (!captured_) {
        if (const auto* t = doc.find_text(id_)) {
            snapshot_ = *t;
            captured_ = true;
        }
    }
    doc.remove_text(id_);
}

void RemoveTextAnnotationCommand::undo(Document& doc) {
    if (captured_) doc.add_text(snapshot_);
}

void ModifyTextAnnotationCommand::apply(Document& doc) {
    auto* t = doc.find_text(id_);
    if (!t) return;
    if (!captured_) {
        before_ = *t;
        captured_ = true;
    }
    after_.id = id_;
    *t = after_;
}

void ModifyTextAnnotationCommand::undo(Document& doc) {
    if (!captured_) return;
    auto* t = doc.find_text(id_);
    if (!t) return;
    *t = before_;
}

}  // namespace cadino::core
