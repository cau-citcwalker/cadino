#include "command/BlockInstanceCommands.hpp"

#include "document/Document.hpp"

namespace cadino::core {

void AddBlockDefinitionCommand::apply(Document& doc) {
    if (!d_.id.valid()) d_.id = next_entity_id();
    doc.add_block_def(d_);
}
void AddBlockDefinitionCommand::undo(Document& doc) {
    doc.remove_block_def(d_.id);
}

void AddBlockInstanceCommand::apply(Document& doc) {
    if (!i_.id.valid()) i_.id = next_entity_id();
    doc.add_block_instance(i_);
}
void AddBlockInstanceCommand::undo(Document& doc) {
    doc.remove_block_instance(i_.id);
}

void RemoveBlockInstanceCommand::apply(Document& doc) {
    if (!captured_) {
        if (const auto* i = doc.find_block_instance(id_)) {
            snapshot_ = *i;
            captured_ = true;
        }
    }
    doc.remove_block_instance(id_);
}
void RemoveBlockInstanceCommand::undo(Document& doc) {
    if (captured_) doc.add_block_instance(snapshot_);
}

void ModifyBlockInstanceCommand::apply(Document& doc) {
    auto* i = doc.find_block_instance(id_);
    if (!i) return;
    if (!captured_) {
        before_ = *i;
        captured_ = true;
    }
    after_.id = id_;
    *i = after_;
}
void ModifyBlockInstanceCommand::undo(Document& doc) {
    if (!captured_) return;
    auto* i = doc.find_block_instance(id_);
    if (!i) return;
    *i = before_;
}

}  // namespace cadino::core
