#pragma once

#include <string_view>

namespace cadino::core {

class Document;

class Command {
public:
    Command() = default;
    virtual ~Command() = default;

    Command(const Command&) = delete;
    Command& operator=(const Command&) = delete;
    Command(Command&&) = delete;
    Command& operator=(Command&&) = delete;

    virtual void apply(Document& doc) = 0;
    virtual void undo(Document& doc) = 0;
    [[nodiscard]] virtual std::string_view name() const noexcept = 0;
};

}  // namespace cadino::core
