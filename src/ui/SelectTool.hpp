#pragma once

#include <QPointF>

#include "Tool.hpp"
#include "entity/Box.hpp"
#include "entity/EntityId.hpp"
#include "entity/Wall.hpp"

namespace cadino::ui {

enum class SelectKind { None, Wall, Box };

class SelectTool : public Tool {
public:
    void on_press(PlanView& view, QPointF model_pos, Qt::MouseButton button) override;
    void on_move(PlanView& view, QPointF model_pos) override;
    void on_release(PlanView& view, QPointF model_pos, Qt::MouseButton button) override;
    void paint_overlay(QPainter& p, const PlanView& view) const override;

    [[nodiscard]] cadino::core::EntityId selected() const noexcept { return selected_; }
    [[nodiscard]] SelectKind kind() const noexcept { return kind_; }

private:
    cadino::core::EntityId selected_{};
    SelectKind kind_{SelectKind::None};
    bool dragging_{false};
    QPointF drag_start_;
    cadino::core::Wall original_wall_{};
    cadino::core::Box original_box_{};
};

}  // namespace cadino::ui
