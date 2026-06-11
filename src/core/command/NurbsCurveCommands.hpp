#pragma once

#include "command/Command.hpp"
#include "entity/NurbsCurve.hpp"

namespace cadino::core {

class AddNurbsCurveCommand : public Command {
public:
    explicit AddNurbsCurveCommand(NurbsCurve curve) noexcept : curve_{std::move(curve)} {}
    void apply(Document& doc) override;
    void undo(Document& doc) override;
    [[nodiscard]] std::string_view name() const noexcept override { return "Add NURBS Curve"; }
    [[nodiscard]] EntityId entity_id() const noexcept { return curve_.id; }

private:
    NurbsCurve curve_;
};

class RemoveNurbsCurveCommand : public Command {
public:
    explicit RemoveNurbsCurveCommand(EntityId id) noexcept : id_{id} {}
    void apply(Document& doc) override;
    void undo(Document& doc) override;
    [[nodiscard]] std::string_view name() const noexcept override { return "Remove NURBS Curve"; }

private:
    EntityId id_;
    NurbsCurve snapshot_{};
    bool captured_{false};
};

class ModifyNurbsCurveCommand : public Command {
public:
    ModifyNurbsCurveCommand(EntityId id, NurbsCurve after) noexcept
        : id_{id}, after_{std::move(after)} {}
    void apply(Document& doc) override;
    void undo(Document& doc) override;
    [[nodiscard]] std::string_view name() const noexcept override { return "Modify NURBS Curve"; }

private:
    EntityId id_;
    NurbsCurve before_{};
    NurbsCurve after_;
    bool captured_{false};
};

}  // namespace cadino::core
