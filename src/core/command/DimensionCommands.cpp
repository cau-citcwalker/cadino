#include "command/DimensionCommands.hpp"

#include "document/Document.hpp"

namespace cadino::core {

void AddDimensionCommand::apply(Document& doc) {
    if (!d_.id.valid()) {
        d_.id = next_entity_id();
    }
    doc.add_dimension(d_);
}

void AddDimensionCommand::undo(Document& doc) {
    doc.remove_dimension(d_.id);
}

void RemoveDimensionCommand::apply(Document& doc) {
    if (!captured_) {
        if (const auto* d = doc.find_dimension(id_)) {
            snapshot_ = *d;
            captured_ = true;
        }
    }
    doc.remove_dimension(id_);
}

void RemoveDimensionCommand::undo(Document& doc) {
    if (captured_) {
        doc.add_dimension(snapshot_);
    }
}

void ModifyDimensionCommand::apply(Document& doc) {
    auto* d = doc.find_dimension(id_);
    if (!d) return;
    if (!captured_) {
        before_ = *d;
        captured_ = true;
    }
    after_.id = id_;
    *d = after_;
}

void ModifyDimensionCommand::undo(Document& doc) {
    if (!captured_) return;
    auto* d = doc.find_dimension(id_);
    if (!d) return;
    *d = before_;
}

}  // namespace cadino::core
