#pragma once

#include "command/Command.hpp"
#include "entity/Block.hpp"

namespace cadino::core {

class AddBlockCommand : public Command {
public:
    explicit AddBlockCommand(Block block) noexcept : block_{std::move(block)} {}
    void apply(Document& doc) override;
    void undo(Document& doc) override;
    [[nodiscard]] std::string_view name() const noexcept override { return "Add Block"; }
    [[nodiscard]] EntityId entity_id() const noexcept { return block_.id; }

private:
    Block block_;
};

class RemoveBlockCommand : public Command {
public:
    explicit RemoveBlockCommand(EntityId id) noexcept : id_{id} {}
    void apply(Document& doc) override;
    void undo(Document& doc) override;
    [[nodiscard]] std::string_view name() const noexcept override { return "Remove Block"; }

private:
    EntityId id_;
    Block snapshot_{};
    bool captured_{false};
};

class ModifyBlockCommand : public Command {
public:
    ModifyBlockCommand(EntityId id, Block after) noexcept
        : id_{id}, after_{std::move(after)} {}
    void apply(Document& doc) override;
    void undo(Document& doc) override;
    [[nodiscard]] std::string_view name() const noexcept override { return "Modify Block"; }

private:
    EntityId id_;
    Block before_{};
    Block after_;
    bool captured_{false};
};

}  // namespace cadino::core
