#pragma once

#include <QPointF>
#include <QString>

#include "Tool.hpp"

namespace cadino::ui {

// One-click text placement tool. After activation the user clicks on
// the plan view to fix the anchor; the text content was supplied at
// activation time via an input dialog.
class TextTool : public Tool {
public:
    explicit TextTool(QString text, double height = 120.0)
        : text_{std::move(text)}, height_{height} {}

    void on_press(PlanView& view, QPointF model_pos, Qt::MouseButton button) override;
    void on_move(PlanView& view, QPointF model_pos) override;
    void on_release(PlanView& view, QPointF model_pos, Qt::MouseButton button) override;
    void paint_overlay(QPainter& p, const PlanView& view) const override;
    void on_cancel(PlanView& view) override;

private:
    QString text_;
    double height_{120.0};
    QPointF hover_;
};

}  // namespace cadino::ui
