#pragma once

#include "command/Command.hpp"
#include "entity/Dimension.hpp"

namespace cadino::core {

class AddDimensionCommand : public Command {
public:
    explicit AddDimensionCommand(Dimension d) noexcept : d_{std::move(d)} {}
    void apply(Document& doc) override;
    void undo(Document& doc) override;
    [[nodiscard]] std::string_view name() const noexcept override { return "Add Dimension"; }
    [[nodiscard]] EntityId entity_id() const noexcept { return d_.id; }

private:
    Dimension d_;
};

class RemoveDimensionCommand : public Command {
public:
    explicit RemoveDimensionCommand(EntityId id) noexcept : id_{id} {}
    void apply(Document& doc) override;
    void undo(Document& doc) override;
    [[nodiscard]] std::string_view name() const noexcept override { return "Remove Dimension"; }

private:
    EntityId id_;
    Dimension snapshot_{};
    bool captured_{false};
};

class ModifyDimensionCommand : public Command {
public:
    ModifyDimensionCommand(EntityId id, Dimension after) noexcept
        : id_{id}, after_{std::move(after)} {}
    void apply(Document& doc) override;
    void undo(Document& doc) override;
    [[nodiscard]] std::string_view name() const noexcept override { return "Modify Dimension"; }

private:
    EntityId id_;
    Dimension before_{};
    Dimension after_;
    bool captured_{false};
};

}  // namespace cadino::core
