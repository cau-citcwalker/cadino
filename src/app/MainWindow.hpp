#pragma once

#include <QMainWindow>
#include <QString>

#include "command/CommandStack.hpp"
#include "document/Document.hpp"

class QAction;
class QSplitter;
class QWidget;

namespace cadino::ui {
class PlanView;
class PropertiesPanel;
class Viewport3D;
}

namespace cadino::app {

enum class ViewMode {
    PlanOnly,
    FrontOnly,
    SideOnly,
    IsoOnly,
    Split,
    Quad,
};

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private:
    void build_menu();
    void build_toolbar();
    void build_central_widget();

    void activate_select_tool();
    void activate_wall_tool();
    void activate_box_tool();
    void activate_cylinder_tool();
    void activate_door_tool();
    void activate_window_tool();
    void activate_slab_tool();
    void delete_selected();
    void group_selected();
    void ungroup_selected();
    void new_document();
    void open_document();
    bool save_document();
    bool save_document_as();
    void export_dxf();
    void set_view_mode(ViewMode mode);
    void update_undo_redo_actions();

    cadino::core::Document document_;
    cadino::core::CommandStack stack_{document_};

    QSplitter* splitter_{nullptr};
    QSplitter* top_row_{nullptr};
    QSplitter* bottom_row_{nullptr};
    cadino::ui::PlanView* plan_view_{nullptr};
    cadino::ui::Viewport3D* viewport_3d_{nullptr};
    cadino::ui::Viewport3D* front_view_{nullptr};
    cadino::ui::Viewport3D* side_view_{nullptr};
    cadino::ui::PropertiesPanel* properties_{nullptr};

    QAction* undo_action_{nullptr};
    QAction* redo_action_{nullptr};
    QAction* delete_action_{nullptr};
    QAction* select_action_{nullptr};
    QAction* wall_action_{nullptr};
    QAction* box_action_{nullptr};
    QAction* cylinder_action_{nullptr};
    QAction* door_action_{nullptr};
    QAction* window_action_{nullptr};
    QAction* slab_action_{nullptr};
    QAction* mode_plan_action_{nullptr};
    QAction* mode_front_action_{nullptr};
    QAction* mode_side_action_{nullptr};
    QAction* mode_iso_action_{nullptr};
    QAction* mode_split_action_{nullptr};
    QAction* mode_quad_action_{nullptr};

    ViewMode view_mode_{ViewMode::Split};
    QString current_file_path_{};
};

}  // namespace cadino::app
