#pragma once

#include "command/Command.hpp"
#include "entity/Box.hpp"

namespace cadino::core {

class AddBoxCommand : public Command {
public:
    explicit AddBoxCommand(Box box) noexcept : box_{std::move(box)} {}
    void apply(Document& doc) override;
    void undo(Document& doc) override;
    [[nodiscard]] std::string_view name() const noexcept override { return "Add Box"; }
    [[nodiscard]] EntityId entity_id() const noexcept { return box_.id; }

private:
    Box box_;
};

class RemoveBoxCommand : public Command {
public:
    explicit RemoveBoxCommand(EntityId id) noexcept : id_{id} {}
    void apply(Document& doc) override;
    void undo(Document& doc) override;
    [[nodiscard]] std::string_view name() const noexcept override { return "Remove Box"; }

private:
    EntityId id_;
    Box snapshot_{};
    bool captured_{false};
};

class ModifyBoxCommand : public Command {
public:
    ModifyBoxCommand(EntityId id, Box after) noexcept
        : id_{id}, after_{std::move(after)} {}
    void apply(Document& doc) override;
    void undo(Document& doc) override;
    [[nodiscard]] std::string_view name() const noexcept override { return "Modify Box"; }

private:
    EntityId id_;
    Box before_{};
    Box after_;
    bool captured_{false};
};

}  // namespace cadino::core
