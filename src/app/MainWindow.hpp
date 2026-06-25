#pragma once

#include <QMainWindow>
#include <QString>

#include "command/CommandStack.hpp"
#include "document/Document.hpp"

class QAction;
class QSplitter;
class QWidget;

namespace cadino::ui {
class LayerPanel;
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

    // Testing accessors — allow GUI tests to drive interactions and inspect
    // widget state without going through file/colour dialogs.
    [[nodiscard]] cadino::ui::PlanView* plan_view_widget() noexcept { return plan_view_; }
    [[nodiscard]] cadino::ui::Viewport3D* viewport_3d_widget() noexcept { return viewport_3d_; }
    [[nodiscard]] cadino::ui::PropertiesPanel* properties_panel_widget() noexcept { return properties_; }
    [[nodiscard]] cadino::ui::LayerPanel* layer_panel_widget() noexcept { return layer_panel_; }
    [[nodiscard]] cadino::core::Document& document() noexcept { return document_; }
    [[nodiscard]] cadino::core::CommandStack& command_stack() noexcept { return stack_; }

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
    void activate_curve_tool();
    void activate_surface_tool();
    void activate_dimension_tool();
    void activate_mirror_tool();
    void rectangular_array_dialog();
    void polar_array_dialog();
    void copy_selected();
    void paste_clipboard();
    void duplicate_selected();
    void delete_selected();
    void group_selected();
    void ungroup_selected();
    void make_block_from_selected();
    void explode_selected_block();
    void define_block_from_selected();
    void insert_block_instance();
    void subtract_selected();
    void union_selected();
    void intersect_selected();
    void new_document();
    void open_document();
    bool save_document();
    bool save_document_as();
    void export_dxf();
    void import_3dm();
    void export_3dm();
    void export_obj();
    void export_stl();
    void export_ifc();
    void export_elevation_dxf(int plane_index);  // 0=Front, 1=Back, 2=Left, 3=Right
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
    cadino::ui::LayerPanel* layer_panel_{nullptr};

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
    QAction* curve_action_{nullptr};
    QAction* surface_action_{nullptr};
    QAction* dimension_action_{nullptr};
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
