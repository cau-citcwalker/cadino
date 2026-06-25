#pragma once

#include "command/Command.hpp"
#include "entity/Leader.hpp"

namespace cadino::core {

class AddLeaderCommand : public Command {
public:
    explicit AddLeaderCommand(Leader l) noexcept : l_{std::move(l)} {}
    void apply(Document& doc) override;
    void undo(Document& doc) override;
    [[nodiscard]] std::string_view name() const noexcept override { return "Add Leader"; }
    [[nodiscard]] EntityId entity_id() const noexcept { return l_.id; }

private:
    Leader l_;
};

class RemoveLeaderCommand : public Command {
public:
    explicit RemoveLeaderCommand(EntityId id) noexcept : id_{id} {}
    void apply(Document& doc) override;
    void undo(Document& doc) override;
    [[nodiscard]] std::string_view name() const noexcept override { return "Remove Leader"; }

private:
    EntityId id_;
    Leader snapshot_{};
    bool captured_{false};
};

class ModifyLeaderCommand : public Command {
public:
    ModifyLeaderCommand(EntityId id, Leader after) noexcept
        : id_{id}, after_{std::move(after)} {}
    void apply(Document& doc) override;
    void undo(Document& doc) override;
    [[nodiscard]] std::string_view name() const noexcept override { return "Modify Leader"; }

private:
    EntityId id_;
    Leader before_{};
    Leader after_;
    bool captured_{false};
};

}  // namespace cadino::core
