#include "command/NurbsSurfaceCommands.hpp"

#include "document/Document.hpp"

namespace cadino::core {

void AddNurbsSurfaceCommand::apply(Document& doc) {
    if (!s_.id.valid()) s_.id = next_entity_id();
    doc.add_surface(s_);
}
void AddNurbsSurfaceCommand::undo(Document& doc) { doc.remove_surface(s_.id); }

void RemoveNurbsSurfaceCommand::apply(Document& doc) {
    if (!captured_) {
        if (const auto* s = doc.find_surface(id_)) {
            snapshot_ = *s;
            captured_ = true;
        }
    }
    doc.remove_surface(id_);
}
void RemoveNurbsSurfaceCommand::undo(Document& doc) {
    if (captured_) doc.add_surface(snapshot_);
}

void ModifyNurbsSurfaceCommand::apply(Document& doc) {
    auto* s = doc.find_surface(id_);
    if (!s) return;
    if (!captured_) {
        before_ = *s;
        captured_ = true;
    }
    after_.id = id_;
    *s = after_;
}
void ModifyNurbsSurfaceCommand::undo(Document& doc) {
    if (!captured_) return;
    auto* s = doc.find_surface(id_);
    if (!s) return;
    *s = before_;
}

}  // namespace cadino::core
