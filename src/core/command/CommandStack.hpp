#pragma once

#include <memory>
#include <vector>

#include "command/Command.hpp"

namespace cadino::core {

class Document;

class CommandStack {
public:
    explicit CommandStack(Document& document) noexcept : document_{document} {}

    void execute(std::unique_ptr<Command> cmd);
    void undo();
    void redo();
    void clear() noexcept;

    [[nodiscard]] bool can_undo() const noexcept { return cursor_ > 0; }
    [[nodiscard]] bool can_redo() const noexcept { return cursor_ < stack_.size(); }
    [[nodiscard]] std::size_t size() const noexcept { return stack_.size(); }
    [[nodiscard]] std::size_t cursor() const noexcept { return cursor_; }

private:
    Document& document_;
    std::vector<std::unique_ptr<Command>> stack_;
    std::size_t cursor_{0};
};

}  // namespace cadino::core
