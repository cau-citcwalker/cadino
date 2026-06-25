#pragma once

#include "command/Command.hpp"
#include "entity/RadialDimension.hpp"

namespace cadino::core {

class AddRadialDimensionCommand : public Command {
public:
    explicit AddRadialDimensionCommand(RadialDimension r) noexcept : r_{std::move(r)} {}
    void apply(Document& doc) override;
    void undo(Document& doc) override;
    [[nodiscard]] std::string_view name() const noexcept override { return "Add Radial Dim"; }
    [[nodiscard]] EntityId entity_id() const noexcept { return r_.id; }

private:
    RadialDimension r_;
};

class RemoveRadialDimensionCommand : public Command {
public:
    explicit RemoveRadialDimensionCommand(EntityId id) noexcept : id_{id} {}
    void apply(Document& doc) override;
    void undo(Document& doc) override;
    [[nodiscard]] std::string_view name() const noexcept override { return "Remove Radial Dim"; }

private:
    EntityId id_;
    RadialDimension snapshot_{};
    bool captured_{false};
};

}  // namespace cadino::core
