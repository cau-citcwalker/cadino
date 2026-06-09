#include "command/DoorWindowSlabCommands.hpp"

#include "document/Document.hpp"

namespace cadino::core {

void AddDoorCommand::apply(Document& doc) {
    if (!door_.id.valid()) door_.id = next_entity_id();
    doc.add_door(door_);
}
void AddDoorCommand::undo(Document& doc) { doc.remove_door(door_.id); }

void RemoveDoorCommand::apply(Document& doc) {
    if (!captured_) {
        if (const auto* d = doc.find_door(id_)) { snapshot_ = *d; captured_ = true; }
    }
    doc.remove_door(id_);
}
void RemoveDoorCommand::undo(Document& doc) { if (captured_) doc.add_door(snapshot_); }

void ModifyDoorCommand::apply(Document& doc) {
    auto* d = doc.find_door(id_);
    if (!d) return;
    if (!captured_) { before_ = *d; captured_ = true; }
    after_.id = id_;
    *d = after_;
}
void ModifyDoorCommand::undo(Document& doc) {
    if (!captured_) return;
    auto* d = doc.find_door(id_);
    if (d) *d = before_;
}

void AddWindowCommand::apply(Document& doc) {
    if (!window_.id.valid()) window_.id = next_entity_id();
    doc.add_window(window_);
}
void AddWindowCommand::undo(Document& doc) { doc.remove_window(window_.id); }

void RemoveWindowCommand::apply(Document& doc) {
    if (!captured_) {
        if (const auto* w = doc.find_window(id_)) { snapshot_ = *w; captured_ = true; }
    }
    doc.remove_window(id_);
}
void RemoveWindowCommand::undo(Document& doc) { if (captured_) doc.add_window(snapshot_); }

void ModifyWindowCommand::apply(Document& doc) {
    auto* w = doc.find_window(id_);
    if (!w) return;
    if (!captured_) { before_ = *w; captured_ = true; }
    after_.id = id_;
    *w = after_;
}
void ModifyWindowCommand::undo(Document& doc) {
    if (!captured_) return;
    auto* w = doc.find_window(id_);
    if (w) *w = before_;
}

void AddSlabCommand::apply(Document& doc) {
    if (!slab_.id.valid()) slab_.id = next_entity_id();
    doc.add_slab(slab_);
}
void AddSlabCommand::undo(Document& doc) { doc.remove_slab(slab_.id); }

void RemoveSlabCommand::apply(Document& doc) {
    if (!captured_) {
        if (const auto* s = doc.find_slab(id_)) { snapshot_ = *s; captured_ = true; }
    }
    doc.remove_slab(id_);
}
void RemoveSlabCommand::undo(Document& doc) { if (captured_) doc.add_slab(snapshot_); }

}  // namespace cadino::core
