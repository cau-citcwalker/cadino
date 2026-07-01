#include "LineTool.hpp"

#include <cmath>

#include <QApplication>
#include <QKeyEvent>
#include <QPainter>

#include "PlanView.hpp"
#include "command/CommandStack.hpp"
#include "command/NurbsCurveCommands.hpp"
#include "entity/NurbsCurve.hpp"

namespace cadino::ui {

namespace {

// Snap `to` so the segment from `from` runs along the world X or Y axis,
// whichever is closer to the raw direction.
QPointF snap_axis(QPointF from, QPointF to) {
    const double dx = to.x() - from.x();
    const double dy = to.y() - from.y();
    if (std::abs(dx) >= std::abs(dy)) {
        return QPointF(to.x(), from.y());
    }
    return QPointF(from.x(), to.y());
}

}  // namespace

QPointF LineTool::apply_ortho(QPointF from, QPointF to, bool ortho_active) const {
    return ortho_active ? snap_axis(from, to) : to;
}

void LineTool::on_press(PlanView& view, QPointF model_pos, Qt::MouseButton button) {
    if (button == Qt::RightButton) {
        finish(view);
        return;
    }
    if (button != Qt::LeftButton) return;

    const bool shift = (QApplication::keyboardModifiers() & Qt::ShiftModifier) != 0;
    const bool ortho = view.ortho_enabled() != shift;  // XOR — shift toggles

    QPointF endpoint = model_pos;
    if (!anchors_.empty()) {
        endpoint = apply_ortho(anchors_.back(), model_pos, ortho);
    }

    anchors_.push_back(endpoint);
    hover_ = endpoint;
    distance_input_.clear();
    view.update();
}

void LineTool::on_move(PlanView& view, QPointF model_pos) {
    hover_ = model_pos;
    if (!anchors_.empty()) view.update();
}

void LineTool::on_release(PlanView&, QPointF, Qt::MouseButton) {}

void LineTool::on_cancel(PlanView& view) {
    anchors_.clear();
    distance_input_.clear();
    view.update();
}

bool LineTool::on_key(PlanView& view, QKeyEvent* event) {
    const int key = event->key();

    if (key == Qt::Key_Return || key == Qt::Key_Enter) {
        if (anchors_.empty()) return false;

        if (!distance_input_.isEmpty()) {
            // Direct distance entry — extend from last anchor along cursor
            // direction (or ortho'd cursor direction) by the typed distance.
            bool ok = false;
            const double dist = distance_input_.toDouble(&ok);
            distance_input_.clear();
            if (!ok || dist <= 0.0) {
                view.update();
                return true;
            }
            const bool shift = (QApplication::keyboardModifiers() & Qt::ShiftModifier) != 0;
            const bool ortho = view.ortho_enabled() != shift;
            const QPointF from = anchors_.back();
            const QPointF dir_target = apply_ortho(from, hover_, ortho);
            const double dx = dir_target.x() - from.x();
            const double dy = dir_target.y() - from.y();
            const double len = std::hypot(dx, dy);
            if (len < 1e-9) {
                view.update();
                return true;
            }
            const QPointF endpoint(from.x() + dx / len * dist,
                                   from.y() + dy / len * dist);
            anchors_.push_back(endpoint);
            hover_ = endpoint;
            view.update();
            return true;
        }

        // No pending distance — finalize the polyline.
        finish(view);
        return true;
    }

    if (key == Qt::Key_Backspace) {
        if (!distance_input_.isEmpty()) {
            distance_input_.chop(1);
            view.update();
            return true;
        }
        return false;
    }

    // Only start accumulating a distance once we already have a start point.
    if (!anchors_.empty()) {
        const QString text = event->text();
        if (text.size() == 1) {
            const QChar ch = text.at(0);
            if (ch.isDigit() || ch == '.') {
                if (ch == '.' && distance_input_.contains('.')) {
                    return true;  // ignore second dot
                }
                distance_input_.append(ch);
                view.update();
                return true;
            }
        }
    }

    return false;
}

void LineTool::finish(PlanView& view) {
    if (anchors_.size() < 2) {
        anchors_.clear();
        distance_input_.clear();
        view.update();
        return;
    }
    cadino::core::NurbsCurve curve;
    curve.degree = 1;
    curve.control_points.reserve(anchors_.size());
    for (const QPointF& pt : anchors_) {
        curve.control_points.emplace_back(pt.x(), pt.y(), 0.0);
    }
    view.command_stack().execute(
        std::make_unique<cadino::core::AddNurbsCurveCommand>(std::move(curve)));
    anchors_.clear();
    distance_input_.clear();
    view.notify_document_modified();
}

void LineTool::paint_overlay(QPainter& p, const PlanView& view) const {
    if (anchors_.empty()) return;

    const bool shift = (QApplication::keyboardModifiers() & Qt::ShiftModifier) != 0;
    const bool ortho = view.ortho_enabled() != shift;
    const QPointF preview_end = apply_ortho(anchors_.back(), hover_, ortho);

    QPen segment_pen(QColor(60, 130, 220), 2);
    segment_pen.setCosmetic(true);
    p.setPen(segment_pen);
    p.setBrush(Qt::NoBrush);
    for (std::size_t i = 1; i < anchors_.size(); ++i) {
        p.drawLine(view.model_to_screen(anchors_[i - 1]),
                   view.model_to_screen(anchors_[i]));
    }

    QPen rubber_pen(QColor(60, 130, 220), 2, Qt::DashLine);
    rubber_pen.setCosmetic(true);
    p.setPen(rubber_pen);
    p.drawLine(view.model_to_screen(anchors_.back()),
               view.model_to_screen(preview_end));

    p.setBrush(QColor(60, 130, 220));
    p.setPen(Qt::NoPen);
    for (const QPointF& a : anchors_) {
        p.drawEllipse(view.model_to_screen(a), 4, 4);
    }

    const double dx = preview_end.x() - anchors_.back().x();
    const double dy = preview_end.y() - anchors_.back().y();
    const double live_len = std::hypot(dx, dy);
    QString label;
    if (!distance_input_.isEmpty()) {
        label = QString("dist: %1").arg(distance_input_);
    } else {
        label = QString("L: %1 mm").arg(live_len, 0, 'f', 1);
    }
    if (ortho) label += QStringLiteral("  [ORTHO]");
    label += QStringLiteral("  ·  Enter=finish  ·  type number=direct distance");

    QFont font = p.font();
    font.setBold(true);
    p.setFont(font);
    const QFontMetrics fm(font);
    const QSize ts = fm.size(0, label);
    const QPointF anchor_s = view.model_to_screen(anchors_.back());
    const QRectF bg(anchor_s.x() + 14, anchor_s.y() - ts.height() - 8,
                    ts.width() + 8, ts.height() + 4);
    p.setBrush(QColor(40, 70, 100, 220));
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(bg, 3, 3);
    p.setPen(Qt::white);
    p.drawText(bg, Qt::AlignCenter, label);
}

}  // namespace cadino::ui
