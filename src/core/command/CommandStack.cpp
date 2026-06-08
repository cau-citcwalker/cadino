#include "command/CommandStack.hpp"

namespace cadino::core {

void CommandStack::execute(std::unique_ptr<Command> cmd) {
    if (!cmd) {
        return;
    }
    cmd->apply(document_);

    stack_.erase(stack_.begin() + static_cast<std::ptrdiff_t>(cursor_), stack_.end());
    stack_.push_back(std::move(cmd));
    cursor_ = stack_.size();
}

void CommandStack::undo() {
    if (!can_undo()) {
        return;
    }
    --cursor_;
    stack_[cursor_]->undo(document_);
}

void CommandStack::redo() {
    if (!can_redo()) {
        return;
    }
    stack_[cursor_]->apply(document_);
    ++cursor_;
}

void CommandStack::clear() noexcept {
    stack_.clear();
    cursor_ = 0;
}

}  // namespace cadino::core
