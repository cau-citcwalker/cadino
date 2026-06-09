#pragma once

#include "Tool.hpp"

namespace cadino::ui {

class DoorTool : public Tool {
public:
    explicit DoorTool(bool window_mode = false) : window_mode_{window_mode} {}
    void on_press(PlanView& view, QPointF model_pos, Qt::MouseButton button) override;
    void on_move(PlanView& view, QPointF model_pos) override;
    void on_release(PlanView&, QPointF, Qt::MouseButton) override {}
    void paint_overlay(QPainter& p, const PlanView& view) const override;

private:
    bool window_mode_;
    QPointF hover_{};
    bool hover_valid_{false};
};

}  // namespace cadino::ui
