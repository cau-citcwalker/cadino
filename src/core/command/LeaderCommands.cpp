#include "command/LeaderCommands.hpp"

#include "document/Document.hpp"

namespace cadino::core {

void AddLeaderCommand::apply(Document& doc) {
    if (!l_.id.valid()) l_.id = next_entity_id();
    doc.add_leader(l_);
}
void AddLeaderCommand::undo(Document& doc) { doc.remove_leader(l_.id); }

void RemoveLeaderCommand::apply(Document& doc) {
    if (!captured_) {
        if (const auto* l = doc.find_leader(id_)) { snapshot_ = *l; captured_ = true; }
    }
    doc.remove_leader(id_);
}
void RemoveLeaderCommand::undo(Document& doc) { if (captured_) doc.add_leader(snapshot_); }

void ModifyLeaderCommand::apply(Document& doc) {
    auto* l = doc.find_leader(id_);
    if (!l) return;
    if (!captured_) { before_ = *l; captured_ = true; }
    after_.id = id_;
    *l = after_;
}
void ModifyLeaderCommand::undo(Document& doc) {
    if (!captured_) return;
    auto* l = doc.find_leader(id_);
    if (!l) return;
    *l = before_;
}

}  // namespace cadino::core
