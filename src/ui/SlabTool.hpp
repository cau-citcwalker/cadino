#pragma once

#include <optional>

#include <QPointF>

#include "Tool.hpp"

namespace cadino::ui {

class SlabTool : public Tool {
public:
    void on_press(PlanView& view, QPointF model_pos, Qt::MouseButton button) override;
    void on_move(PlanView& view, QPointF model_pos) override;
    void on_release(PlanView&, QPointF, Qt::MouseButton) override {}
    void paint_overlay(QPainter& p, const PlanView& view) const override;
    void on_cancel(PlanView& view) override;

private:
    std::optional<QPointF> corner1_;
    QPointF hover_{};
};

}  // namespace cadino::ui
