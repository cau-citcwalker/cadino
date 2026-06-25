#pragma once

#include "command/Command.hpp"
#include "entity/AngularDimension.hpp"

namespace cadino::core {

class AddAngularDimensionCommand : public Command {
public:
    explicit AddAngularDimensionCommand(AngularDimension a) noexcept : a_{std::move(a)} {}
    void apply(Document& doc) override;
    void undo(Document& doc) override;
    [[nodiscard]] std::string_view name() const noexcept override { return "Add Angular Dim"; }
    [[nodiscard]] EntityId entity_id() const noexcept { return a_.id; }

private:
    AngularDimension a_;
};

class RemoveAngularDimensionCommand : public Command {
public:
    explicit RemoveAngularDimensionCommand(EntityId id) noexcept : id_{id} {}
    void apply(Document& doc) override;
    void undo(Document& doc) override;
    [[nodiscard]] std::string_view name() const noexcept override { return "Remove Angular Dim"; }

private:
    EntityId id_;
    AngularDimension snapshot_{};
    bool captured_{false};
};

}  // namespace cadino::core
