#pragma once

#include "command/Command.hpp"
#include "entity/TextAnnotation.hpp"

namespace cadino::core {

class AddTextAnnotationCommand : public Command {
public:
    explicit AddTextAnnotationCommand(TextAnnotation t) noexcept : t_{std::move(t)} {}
    void apply(Document& doc) override;
    void undo(Document& doc) override;
    [[nodiscard]] std::string_view name() const noexcept override { return "Add Text"; }
    [[nodiscard]] EntityId entity_id() const noexcept { return t_.id; }

private:
    TextAnnotation t_;
};

class RemoveTextAnnotationCommand : public Command {
public:
    explicit RemoveTextAnnotationCommand(EntityId id) noexcept : id_{id} {}
    void apply(Document& doc) override;
    void undo(Document& doc) override;
    [[nodiscard]] std::string_view name() const noexcept override { return "Remove Text"; }

private:
    EntityId id_;
    TextAnnotation snapshot_{};
    bool captured_{false};
};

class ModifyTextAnnotationCommand : public Command {
public:
    ModifyTextAnnotationCommand(EntityId id, TextAnnotation after) noexcept
        : id_{id}, after_{std::move(after)} {}
    void apply(Document& doc) override;
    void undo(Document& doc) override;
    [[nodiscard]] std::string_view name() const noexcept override { return "Modify Text"; }

private:
    EntityId id_;
    TextAnnotation before_{};
    TextAnnotation after_;
    bool captured_{false};
};

}  // namespace cadino::core
