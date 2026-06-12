#pragma once

#include "command/Command.hpp"
#include "entity/BlockDefinition.hpp"
#include "entity/BlockInstance.hpp"

namespace cadino::core {

class AddBlockDefinitionCommand : public Command {
public:
    explicit AddBlockDefinitionCommand(BlockDefinition d) noexcept : d_{std::move(d)} {}
    void apply(Document& doc) override;
    void undo(Document& doc) override;
    [[nodiscard]] std::string_view name() const noexcept override { return "Add Block Definition"; }
    [[nodiscard]] EntityId entity_id() const noexcept { return d_.id; }

private:
    BlockDefinition d_;
};

class AddBlockInstanceCommand : public Command {
public:
    explicit AddBlockInstanceCommand(BlockInstance i) noexcept : i_{std::move(i)} {}
    void apply(Document& doc) override;
    void undo(Document& doc) override;
    [[nodiscard]] std::string_view name() const noexcept override { return "Add Block Instance"; }
    [[nodiscard]] EntityId entity_id() const noexcept { return i_.id; }

private:
    BlockInstance i_;
};

class RemoveBlockInstanceCommand : public Command {
public:
    explicit RemoveBlockInstanceCommand(EntityId id) noexcept : id_{id} {}
    void apply(Document& doc) override;
    void undo(Document& doc) override;
    [[nodiscard]] std::string_view name() const noexcept override { return "Remove Block Instance"; }

private:
    EntityId id_;
    BlockInstance snapshot_{};
    bool captured_{false};
};

class ModifyBlockInstanceCommand : public Command {
public:
    ModifyBlockInstanceCommand(EntityId id, BlockInstance after) noexcept
        : id_{id}, after_{std::move(after)} {}
    void apply(Document& doc) override;
    void undo(Document& doc) override;
    [[nodiscard]] std::string_view name() const noexcept override { return "Modify Block Instance"; }

private:
    EntityId id_;
    BlockInstance before_{};
    BlockInstance after_;
    bool captured_{false};
};

}  // namespace cadino::core
