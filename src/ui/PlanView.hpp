#pragma once

#include <memory>

#include <QPointF>
#include <QTransform>
#include <QWidget>

#include "Selection.hpp"
#include "Snap.hpp"

namespace cadino::core {
class Document;
class CommandStack;
}  // namespace cadino::core

namespace cadino::ui {

class Tool;

class PlanView : public QWidget {
    Q_OBJECT

public:
    PlanView(cadino::core::Document& doc, cadino::core::CommandStack& stack,
             QWidget* parent = nullptr);
    ~PlanView() override;

    void set_tool(std::unique_ptr<Tool> tool);
    [[nodiscard]] Tool* tool() noexcept { return tool_.get(); }

    [[nodiscard]] cadino::core::Document& document() noexcept { return document_; }
    [[nodiscard]] const cadino::core::Document& document() const noexcept { return document_; }
    [[nodiscard]] cadino::core::CommandStack& command_stack() noexcept { return stack_; }

    [[nodiscard]] QPointF screen_to_model(QPointF screen) const;
    [[nodiscard]] QPointF model_to_screen(QPointF model) const;
    [[nodiscard]] double zoom() const noexcept { return zoom_; }

    void notify_document_modified();

    [[nodiscard]] const Selection& selection() const noexcept { return selection_; }
    void set_selection(Selection sel);
    void clear_selection();

    [[nodiscard]] SnapEngine& snap_engine() noexcept { return snap_; }
    [[nodiscard]] const SnapResult& last_snap() const noexcept { return last_snap_; }

signals:
    void document_modified();
    void selection_changed(const Selection& sel);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void update_transform();
    void draw_grid(QPainter& p);
    void draw_walls(QPainter& p);
    void draw_boxes(QPainter& p);
    void draw_cylinders(QPainter& p);
    void draw_snap_marker(QPainter& p);
    QPointF apply_snap(QPointF model_pos);

    cadino::core::Document& document_;
    cadino::core::CommandStack& stack_;
    std::unique_ptr<Tool> tool_;

    double zoom_{0.1};
    QPointF view_offset_{0.0, 0.0};
    QTransform model_to_screen_;
    QTransform screen_to_model_;

    bool panning_{false};
    QPointF last_pan_screen_{};

    Selection selection_{};

    SnapEngine snap_;
    SnapResult last_snap_{};
};

}  // namespace cadino::ui
