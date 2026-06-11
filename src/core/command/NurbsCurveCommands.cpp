#include "command/NurbsCurveCommands.hpp"

#include "document/Document.hpp"

namespace cadino::core {

void AddNurbsCurveCommand::apply(Document& doc) {
    if (!curve_.id.valid()) curve_.id = next_entity_id();
    doc.add_curve(curve_);
}

void AddNurbsCurveCommand::undo(Document& doc) {
    doc.remove_curve(curve_.id);
}

void RemoveNurbsCurveCommand::apply(Document& doc) {
    if (!captured_) {
        if (const auto* c = doc.find_curve(id_)) {
            snapshot_ = *c;
            captured_ = true;
        }
    }
    doc.remove_curve(id_);
}

void RemoveNurbsCurveCommand::undo(Document& doc) {
    if (captured_) doc.add_curve(snapshot_);
}

void ModifyNurbsCurveCommand::apply(Document& doc) {
    auto* c = doc.find_curve(id_);
    if (!c) return;
    if (!captured_) {
        before_ = *c;
        captured_ = true;
    }
    after_.id = id_;
    *c = after_;
}

void ModifyNurbsCurveCommand::undo(Document& doc) {
    if (!captured_) return;
    auto* c = doc.find_curve(id_);
    if (!c) return;
    *c = before_;
}

}  // namespace cadino::core
