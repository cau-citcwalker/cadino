#pragma once

#include "command/Command.hpp"
#include "entity/Door.hpp"
#include "entity/Slab.hpp"
#include "entity/Window.hpp"

namespace cadino::core {

class AddDoorCommand : public Command {
public:
    explicit AddDoorCommand(Door d) noexcept : door_{d} {}
    void apply(Document& doc) override;
    void undo(Document& doc) override;
    [[nodiscard]] std::string_view name() const noexcept override { return "Add Door"; }
    [[nodiscard]] EntityId entity_id() const noexcept { return door_.id; }
private:
    Door door_;
};

class RemoveDoorCommand : public Command {
public:
    explicit RemoveDoorCommand(EntityId id) noexcept : id_{id} {}
    void apply(Document& doc) override;
    void undo(Document& doc) override;
    [[nodiscard]] std::string_view name() const noexcept override { return "Remove Door"; }
private:
    EntityId id_;
    Door snapshot_{};
    bool captured_{false};
};

class ModifyDoorCommand : public Command {
public:
    ModifyDoorCommand(EntityId id, Door after) noexcept : id_{id}, after_{after} {}
    void apply(Document& doc) override;
    void undo(Document& doc) override;
    [[nodiscard]] std::string_view name() const noexcept override { return "Modify Door"; }
private:
    EntityId id_;
    Door before_{};
    Door after_;
    bool captured_{false};
};

class AddWindowCommand : public Command {
public:
    explicit AddWindowCommand(Window w) noexcept : window_{w} {}
    void apply(Document& doc) override;
    void undo(Document& doc) override;
    [[nodiscard]] std::string_view name() const noexcept override { return "Add Window"; }
    [[nodiscard]] EntityId entity_id() const noexcept { return window_.id; }
private:
    Window window_;
};

class RemoveWindowCommand : public Command {
public:
    explicit RemoveWindowCommand(EntityId id) noexcept : id_{id} {}
    void apply(Document& doc) override;
    void undo(Document& doc) override;
    [[nodiscard]] std::string_view name() const noexcept override { return "Remove Window"; }
private:
    EntityId id_;
    Window snapshot_{};
    bool captured_{false};
};

class ModifyWindowCommand : public Command {
public:
    ModifyWindowCommand(EntityId id, Window after) noexcept : id_{id}, after_{after} {}
    void apply(Document& doc) override;
    void undo(Document& doc) override;
    [[nodiscard]] std::string_view name() const noexcept override { return "Modify Window"; }
private:
    EntityId id_;
    Window before_{};
    Window after_;
    bool captured_{false};
};

class AddSlabCommand : public Command {
public:
    explicit AddSlabCommand(Slab s) noexcept : slab_{std::move(s)} {}
    void apply(Document& doc) override;
    void undo(Document& doc) override;
    [[nodiscard]] std::string_view name() const noexcept override { return "Add Slab"; }
    [[nodiscard]] EntityId entity_id() const noexcept { return slab_.id; }
private:
    Slab slab_;
};

class RemoveSlabCommand : public Command {
public:
    explicit RemoveSlabCommand(EntityId id) noexcept : id_{id} {}
    void apply(Document& doc) override;
    void undo(Document& doc) override;
    [[nodiscard]] std::string_view name() const noexcept override { return "Remove Slab"; }
private:
    EntityId id_;
    Slab snapshot_{};
    bool captured_{false};
};

}  // namespace cadino::core
