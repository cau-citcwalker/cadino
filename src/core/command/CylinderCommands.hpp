#pragma once

#include "command/Command.hpp"
#include "entity/Cylinder.hpp"

namespace cadino::core {

class AddCylinderCommand : public Command {
public:
    explicit AddCylinderCommand(Cylinder cyl) noexcept : cyl_{std::move(cyl)} {}
    void apply(Document& doc) override;
    void undo(Document& doc) override;
    [[nodiscard]] std::string_view name() const noexcept override { return "Add Cylinder"; }
    [[nodiscard]] EntityId entity_id() const noexcept { return cyl_.id; }

private:
    Cylinder cyl_;
};

class RemoveCylinderCommand : public Command {
public:
    explicit RemoveCylinderCommand(EntityId id) noexcept : id_{id} {}
    void apply(Document& doc) override;
    void undo(Document& doc) override;
    [[nodiscard]] std::string_view name() const noexcept override { return "Remove Cylinder"; }

private:
    EntityId id_;
    Cylinder snapshot_{};
    bool captured_{false};
};

class ModifyCylinderCommand : public Command {
public:
    ModifyCylinderCommand(EntityId id, Cylinder after) noexcept
        : id_{id}, after_{std::move(after)} {}
    void apply(Document& doc) override;
    void undo(Document& doc) override;
    [[nodiscard]] std::string_view name() const noexcept override { return "Modify Cylinder"; }

private:
    EntityId id_;
    Cylinder before_{};
    Cylinder after_;
    bool captured_{false};
};

}  // namespace cadino::core
