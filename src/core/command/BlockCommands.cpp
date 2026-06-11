#include "command/BlockCommands.hpp"

#include "document/Document.hpp"

namespace cadino::core {

void AddBlockCommand::apply(Document& doc) {
    if (!block_.id.valid()) block_.id = next_entity_id();
    doc.add_block(block_);
}

void AddBlockCommand::undo(Document& doc) {
    doc.remove_block(block_.id);
}

void RemoveBlockCommand::apply(Document& doc) {
    if (!captured_) {
        if (const auto* b = doc.find_block(id_)) {
            snapshot_ = *b;
            captured_ = true;
        }
    }
    doc.remove_block(id_);
}

void RemoveBlockCommand::undo(Document& doc) {
    if (captured_) doc.add_block(snapshot_);
}

void ModifyBlockCommand::apply(Document& doc) {
    auto* b = doc.find_block(id_);
    if (!b) return;
    if (!captured_) {
        before_ = *b;
        captured_ = true;
    }
    after_.id = id_;
    *b = after_;
}

void ModifyBlockCommand::undo(Document& doc) {
    if (!captured_) return;
    auto* b = doc.find_block(id_);
    if (!b) return;
    *b = before_;
}

}  // namespace cadino::core
