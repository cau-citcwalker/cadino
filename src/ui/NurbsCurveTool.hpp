#pragma once

#include <QPointF>
#include <vector>

#include "Tool.hpp"

namespace cadino::ui {

class NurbsCurveTool : public Tool {
public:
    void on_press(PlanView& view, QPointF model_pos, Qt::MouseButton button) override;
    void on_move(PlanView& view, QPointF model_pos) override;
    void on_release(PlanView& view, QPointF model_pos, Qt::MouseButton button) override;
    void paint_overlay(QPainter& p, const PlanView& view) const override;
    void on_cancel(PlanView& view) override;

    void set_degree(int d) noexcept { degree_ = d; }

private:
    void finish(PlanView& view);

    std::vector<QPointF> control_points_;
    QPointF hover_;
    int degree_{3};
};

}  // namespace cadino::ui
