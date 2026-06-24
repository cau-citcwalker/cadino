#pragma once

#include <QWidget>

class QTableWidget;
class QPushButton;

namespace cadino::core {
class Document;
}

namespace cadino::ui {

class PlanView;
class Viewport3D;

class LayerPanel : public QWidget {
    Q_OBJECT

public:
    LayerPanel(cadino::core::Document& doc, PlanView& view, QWidget* parent = nullptr);

    void register_viewport3d(Viewport3D* v) { viewport_3d_ = v; }

public slots:
    void refresh();

signals:
    void active_layer_changed();
    void layers_changed();

private:
    void rebuild_table();
    void on_cell_changed(int row, int col);
    void on_add_layer();
    void on_remove_layer();
    void on_set_active(int row);
    void emit_view_refresh();

    cadino::core::Document& document_;
    PlanView& view_;
    Viewport3D* viewport_3d_{nullptr};

    QTableWidget* table_{nullptr};
    QPushButton* add_btn_{nullptr};
    QPushButton* remove_btn_{nullptr};

    bool suppress_signals_{false};
};

}  // namespace cadino::ui
