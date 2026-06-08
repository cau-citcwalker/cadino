#pragma once

#include "command/Command.hpp"
#include "entity/Wall.hpp"

namespace cadino::core {

class AddWallCommand : public Command {
public:
    explicit AddWallCommand(Wall wall) noexcept : wall_{std::move(wall)} {}

    void apply(Document& doc) override;
    void undo(Document& doc) override;
    [[nodiscard]] std::string_view name() const noexcept override { return "Add Wall"; }

    [[nodiscard]] EntityId entity_id() const noexcept { return wall_.id; }

private:
    Wall wall_;
};

class RemoveWallCommand : public Command {
public:
    explicit RemoveWallCommand(EntityId id) noexcept : id_{id} {}

    void apply(Document& doc) override;
    void undo(Document& doc) override;
    [[nodiscard]] std::string_view name() const noexcept override { return "Remove Wall"; }

private:
    EntityId id_;
    Wall snapshot_{};
    bool captured_{false};
};

class ModifyWallCommand : public Command {
public:
    ModifyWallCommand(EntityId id, Wall after) noexcept
        : id_{id}, after_{std::move(after)} {}

    void apply(Document& doc) override;
    void undo(Document& doc) override;
    [[nodiscard]] std::string_view name() const noexcept override { return "Modify Wall"; }

private:
    EntityId id_;
    Wall before_{};
    Wall after_;
    bool captured_{false};
};

}  // namespace cadino::core
