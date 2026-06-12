#pragma once

#include "command/Command.hpp"
#include "entity/NurbsSurface.hpp"

namespace cadino::core {

class AddNurbsSurfaceCommand : public Command {
public:
    explicit AddNurbsSurfaceCommand(NurbsSurface s) noexcept : s_{std::move(s)} {}
    void apply(Document& doc) override;
    void undo(Document& doc) override;
    [[nodiscard]] std::string_view name() const noexcept override { return "Add NURBS Surface"; }
    [[nodiscard]] EntityId entity_id() const noexcept { return s_.id; }

private:
    NurbsSurface s_;
};

class RemoveNurbsSurfaceCommand : public Command {
public:
    explicit RemoveNurbsSurfaceCommand(EntityId id) noexcept : id_{id} {}
    void apply(Document& doc) override;
    void undo(Document& doc) override;
    [[nodiscard]] std::string_view name() const noexcept override { return "Remove NURBS Surface"; }

private:
    EntityId id_;
    NurbsSurface snapshot_{};
    bool captured_{false};
};

class ModifyNurbsSurfaceCommand : public Command {
public:
    ModifyNurbsSurfaceCommand(EntityId id, NurbsSurface after) noexcept
        : id_{id}, after_{std::move(after)} {}
    void apply(Document& doc) override;
    void undo(Document& doc) override;
    [[nodiscard]] std::string_view name() const noexcept override { return "Modify NURBS Surface"; }

private:
    EntityId id_;
    NurbsSurface before_{};
    NurbsSurface after_;
    bool captured_{false};
};

}  // namespace cadino::core
