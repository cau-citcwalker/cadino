#include "TextTool.hpp"

#include <QPainter>

#include "PlanView.hpp"
#include "command/CommandStack.hpp"
#include "command/TextAnnotationCommands.hpp"

namespace cadino::ui {

void TextTool::on_press(PlanView& view, QPointF model_pos, Qt::MouseButton button) {
    if (button != Qt::LeftButton) return;
    cadino::core::TextAnnotation t;
    t.position = {model_pos.x(), model_pos.y()};
    t.text = text_.toStdString();
    t.height = height_;
    view.command_stack().execute(
        std::make_unique<cadino::core::AddTextAnnotationCommand>(std::move(t)));
    view.notify_document_modified();
}

void TextTool::on_move(PlanView& view, QPointF model_pos) {
    hover_ = model_pos;
    view.update();
}

void TextTool::on_release(PlanView&, QPointF, Qt::MouseButton) {}
void TextTool::on_cancel(PlanView&) {}

void TextTool::paint_overlay(QPainter& p, const PlanView& view) const {
    const QPointF s = view.model_to_screen(hover_);
    QFont font = p.font();
    font.setPointSizeF(std::max(8.0, height_ * view.zoom() * 0.6));
    font.setBold(true);
    p.setFont(font);
    p.setPen(QColor(220, 220, 220, 180));
    p.drawText(s, text_);
}

}  // namespace cadino::ui
