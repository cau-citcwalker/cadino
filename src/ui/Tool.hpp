#pragma once

#include <QPainter>
#include <QPointF>

namespace cadino::ui {

class PlanView;

class Tool {
public:
    virtual ~Tool() = default;

    virtual void on_press(PlanView& view, QPointF model_pos, Qt::MouseButton button) = 0;
    virtual void on_move(PlanView& view, QPointF model_pos) = 0;
    virtual void on_release(PlanView& view, QPointF model_pos, Qt::MouseButton button) = 0;

    virtual void paint_overlay(QPainter& /*p*/, const PlanView& /*view*/) const {}

    virtual void on_cancel(PlanView& /*view*/) {}
};

}  // namespace cadino::ui
