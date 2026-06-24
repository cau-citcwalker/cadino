#include "MainWindow.hpp"

#include <QAction>
#include <QActionGroup>
#include <QDockWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QInputDialog>
#include <QLineEdit>
#include <QKeySequence>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QSplitter>
#include <QStatusBar>
#include <QTimer>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWidget>

#include <spdlog/spdlog.h>

#include "BooleanOps.hpp"
#include "BoxTool.hpp"
#include "DimensionTool.hpp"
#include "LayerPanel.hpp"
#include "CylinderTool.hpp"
#include "DocumentIO.hpp"
#include "DoorTool.hpp"
#include "DxfExporter.hpp"
#include "MeshExport.hpp"
#include "PlanView.hpp"
#include "PropertiesPanel.hpp"
#include "SelectTool.hpp"
#include "NurbsCurveTool.hpp"
#include "NurbsSurfaceTool.hpp"
#include "SlabTool.hpp"
#include "Viewport3D.hpp"
#include "WallTool.hpp"
#include <QKeySequence>

#include "command/BlockCommands.hpp"
#include "command/BlockInstanceCommands.hpp"
#include "command/BoxCommands.hpp"
#include "command/CylinderCommands.hpp"
#include "command/DimensionCommands.hpp"
#include "command/NurbsCurveCommands.hpp"
#include "command/NurbsSurfaceCommands.hpp"

#ifdef CADINO_HAS_OPENNURBS
#include "RhinoIO.hpp"
#endif
#include "command/WallCommands.hpp"

namespace cadino::app {

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("Cadino");
    resize(1280, 800);

    build_central_widget();
    build_menu();
    build_toolbar();

    properties_ = new cadino::ui::PropertiesPanel(document_, stack_, *plan_view_, this);
    auto* dock = new QDockWidget("Properties", this);
    dock->setWidget(properties_);
    dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    addDockWidget(Qt::RightDockWidgetArea, dock);

    layer_panel_ = new cadino::ui::LayerPanel(document_, *plan_view_, this);
    layer_panel_->register_viewport3d(viewport_3d_);
    auto* layer_dock = new QDockWidget("Layers", this);
    layer_dock->setWidget(layer_panel_);
    layer_dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    addDockWidget(Qt::RightDockWidgetArea, layer_dock);

    connect(plan_view_, &cadino::ui::PlanView::selection_changed, this, [this] {
        properties_->set_selection(plan_view_->primary_selection());
    });
    connect(plan_view_, &cadino::ui::PlanView::document_modified, properties_,
            &cadino::ui::PropertiesPanel::refresh);
    connect(plan_view_, &cadino::ui::PlanView::document_modified, layer_panel_,
            &cadino::ui::LayerPanel::refresh);

    activate_select_tool();
    // Defer the initial mode switch until after Qt finishes the first window
    // layout. Calling set_view_mode (which toggles widget visibility) during
    // construction races with the QSplitter measure pass and leaves both
    // viewports stuck at 0 size.
    QTimer::singleShot(0, this, [this] { set_view_mode(ViewMode::Split); });
    statusBar()->showMessage("Ready");
    spdlog::info("MainWindow ready");
}

MainWindow::~MainWindow() = default;

void MainWindow::build_central_widget() {
    splitter_ = new QSplitter(Qt::Vertical, this);
    splitter_->setHandleWidth(4);
    splitter_->setChildrenCollapsible(false);

    top_row_ = new QSplitter(Qt::Horizontal, splitter_);
    top_row_->setHandleWidth(4);
    top_row_->setChildrenCollapsible(false);

    bottom_row_ = new QSplitter(Qt::Horizontal, splitter_);
    bottom_row_->setHandleWidth(4);
    bottom_row_->setChildrenCollapsible(false);

    plan_view_ = new cadino::ui::PlanView(document_, stack_, top_row_);
    viewport_3d_ = new cadino::ui::Viewport3D(document_, stack_, *plan_view_, top_row_);
    front_view_ = new cadino::ui::Viewport3D(document_, stack_, *plan_view_, bottom_row_);
    side_view_ = new cadino::ui::Viewport3D(document_, stack_, *plan_view_, bottom_row_);

    front_view_->set_preset(cadino::ui::Viewport3D::CameraPreset::Front);
    side_view_->set_preset(cadino::ui::Viewport3D::CameraPreset::Right);

    auto refresh_all_3d = [this] {
        viewport_3d_->refresh();
        front_view_->refresh();
        side_view_->refresh();
    };
    connect(plan_view_, &cadino::ui::PlanView::document_modified, this, [this, refresh_all_3d] {
        refresh_all_3d();
        update_undo_redo_actions();
    });
    connect(plan_view_, &cadino::ui::PlanView::selection_changed, this, refresh_all_3d);

    plan_view_->setMinimumSize(200, 150);
    viewport_3d_->setMinimumSize(200, 150);
    front_view_->setMinimumSize(200, 150);
    side_view_->setMinimumSize(200, 150);

    top_row_->addWidget(plan_view_);
    top_row_->addWidget(viewport_3d_);
    top_row_->setStretchFactor(0, 1);
    top_row_->setStretchFactor(1, 1);

    bottom_row_->addWidget(front_view_);
    bottom_row_->addWidget(side_view_);
    bottom_row_->setStretchFactor(0, 1);
    bottom_row_->setStretchFactor(1, 1);

    splitter_->addWidget(top_row_);
    splitter_->addWidget(bottom_row_);
    splitter_->setStretchFactor(0, 1);
    splitter_->setStretchFactor(1, 1);

    setCentralWidget(splitter_);
}

void MainWindow::build_menu() {
    auto* file_menu = menuBar()->addMenu("&File");
    auto* new_a = file_menu->addAction("&New");
    new_a->setShortcut(QKeySequence::New);
    connect(new_a, &QAction::triggered, this, &MainWindow::new_document);

    auto* open_a = file_menu->addAction("&Open...");
    open_a->setShortcut(QKeySequence::Open);
    connect(open_a, &QAction::triggered, this, &MainWindow::open_document);

    auto* save_a = file_menu->addAction("&Save");
    save_a->setShortcut(QKeySequence::Save);
    connect(save_a, &QAction::triggered, this, [this] { (void)save_document(); });

    auto* save_as_a = file_menu->addAction("Save &As...");
    save_as_a->setShortcut(QKeySequence::SaveAs);
    connect(save_as_a, &QAction::triggered, this, [this] { (void)save_document_as(); });

    file_menu->addSeparator();
    auto* elevations_menu = file_menu->addMenu("Export &Elevation");
    auto* front_a = elevations_menu->addAction("&Front (XZ)");
    connect(front_a, &QAction::triggered, this, [this] { export_elevation_dxf(0); });
    auto* back_a = elevations_menu->addAction("&Back (XZ, mirrored)");
    connect(back_a, &QAction::triggered, this, [this] { export_elevation_dxf(1); });
    auto* left_a = elevations_menu->addAction("&Left (YZ)");
    connect(left_a, &QAction::triggered, this, [this] { export_elevation_dxf(2); });
    auto* right_a = elevations_menu->addAction("&Right (YZ)");
    connect(right_a, &QAction::triggered, this, [this] { export_elevation_dxf(3); });

    file_menu->addSeparator();
    auto* import_3dm_a = file_menu->addAction("&Import Rhino 3DM...");
    connect(import_3dm_a, &QAction::triggered, this, &MainWindow::import_3dm);

    auto* export_3dm_a = file_menu->addAction("E&xport Rhino 3DM...");
    connect(export_3dm_a, &QAction::triggered, this, &MainWindow::export_3dm);

    auto* export_dxf_a = file_menu->addAction("Export &DXF...");
    connect(export_dxf_a, &QAction::triggered, this, &MainWindow::export_dxf);

    auto* export_obj_a = file_menu->addAction("Export &OBJ...");
    connect(export_obj_a, &QAction::triggered, this, &MainWindow::export_obj);

    auto* export_stl_a = file_menu->addAction("Export S&TL...");
    connect(export_stl_a, &QAction::triggered, this, &MainWindow::export_stl);

#ifndef CADINO_HAS_OPENNURBS
    import_3dm_a->setEnabled(false);
    export_3dm_a->setEnabled(false);
    import_3dm_a->setToolTip("Built without openNURBS support");
    export_3dm_a->setToolTip("Built without openNURBS support");
#endif

    file_menu->addSeparator();
    file_menu->addAction("E&xit", this, &QWidget::close);

    auto* edit_menu = menuBar()->addMenu("&Edit");
    undo_action_ = edit_menu->addAction("&Undo");
    undo_action_->setShortcut(QKeySequence::Undo);
    connect(undo_action_, &QAction::triggered, this, [this] {
        stack_.undo();
        plan_view_->update();
        viewport_3d_->update();
        update_undo_redo_actions();
    });
    redo_action_ = edit_menu->addAction("&Redo");
    redo_action_->setShortcuts({QKeySequence("Ctrl+Shift+Z"), QKeySequence("Ctrl+Y")});
    connect(redo_action_, &QAction::triggered, this, [this] {
        stack_.redo();
        plan_view_->update();
        viewport_3d_->update();
        update_undo_redo_actions();
    });
    update_undo_redo_actions();

    edit_menu->addSeparator();
    delete_action_ = edit_menu->addAction("&Delete");
    delete_action_->setShortcuts({QKeySequence::Delete, QKeySequence(Qt::Key_Backspace)});
    connect(delete_action_, &QAction::triggered, this, &MainWindow::delete_selected);

    edit_menu->addSeparator();
    auto* group_a = edit_menu->addAction("&Group");
    group_a->setShortcut(QKeySequence("Ctrl+G"));
    connect(group_a, &QAction::triggered, this, &MainWindow::group_selected);

    auto* ungroup_a = edit_menu->addAction("Un&group");
    ungroup_a->setShortcut(QKeySequence("Ctrl+Shift+G"));
    connect(ungroup_a, &QAction::triggered, this, &MainWindow::ungroup_selected);

    auto* subtract_a = edit_menu->addAction("Su&btract");
    subtract_a->setShortcut(QKeySequence("Ctrl+Minus"));
    connect(subtract_a, &QAction::triggered, this, &MainWindow::subtract_selected);

    auto* union_a = edit_menu->addAction("&Union");
    union_a->setShortcut(QKeySequence("Ctrl+Shift+U"));
    connect(union_a, &QAction::triggered, this, &MainWindow::union_selected);

    auto* intersect_a = edit_menu->addAction("&Intersect");
    intersect_a->setShortcut(QKeySequence("Ctrl+Shift+I"));
    connect(intersect_a, &QAction::triggered, this, &MainWindow::intersect_selected);

    edit_menu->addSeparator();
    auto* make_block_a = edit_menu->addAction("Make &Block");
    make_block_a->setShortcut(QKeySequence("Ctrl+Alt+G"));
    connect(make_block_a, &QAction::triggered, this, &MainWindow::make_block_from_selected);

    auto* explode_a = edit_menu->addAction("E&xplode Block");
    explode_a->setShortcut(QKeySequence("Ctrl+Alt+X"));
    connect(explode_a, &QAction::triggered, this, &MainWindow::explode_selected_block);

    auto* define_block_a = edit_menu->addAction("&Define Block from Selection");
    define_block_a->setShortcut(QKeySequence("Ctrl+Alt+D"));
    connect(define_block_a, &QAction::triggered, this, &MainWindow::define_block_from_selected);

    auto* insert_inst_a = edit_menu->addAction("&Insert Block Instance...");
    insert_inst_a->setShortcut(QKeySequence("Ctrl+Alt+I"));
    connect(insert_inst_a, &QAction::triggered, this, &MainWindow::insert_block_instance);

    auto* view_menu = menuBar()->addMenu("&View");
    mode_plan_action_ = view_menu->addAction("&Plan (Top)");
    mode_plan_action_->setCheckable(true);
    mode_plan_action_->setShortcut(QKeySequence("F2"));
    connect(mode_plan_action_, &QAction::triggered, this,
            [this] { set_view_mode(ViewMode::PlanOnly); });

    mode_front_action_ = view_menu->addAction("&Front");
    mode_front_action_->setCheckable(true);
    mode_front_action_->setShortcut(QKeySequence("F3"));
    connect(mode_front_action_, &QAction::triggered, this,
            [this] { set_view_mode(ViewMode::FrontOnly); });

    mode_side_action_ = view_menu->addAction("S&ide");
    mode_side_action_->setCheckable(true);
    mode_side_action_->setShortcut(QKeySequence("F4"));
    connect(mode_side_action_, &QAction::triggered, this,
            [this] { set_view_mode(ViewMode::SideOnly); });

    mode_iso_action_ = view_menu->addAction("&3D");
    mode_iso_action_->setCheckable(true);
    mode_iso_action_->setShortcut(QKeySequence("F5"));
    connect(mode_iso_action_, &QAction::triggered, this,
            [this] { set_view_mode(ViewMode::IsoOnly); });

    mode_split_action_ = view_menu->addAction("&Split (2D + 3D)");
    mode_split_action_->setCheckable(true);
    mode_split_action_->setShortcut(QKeySequence("F6"));
    connect(mode_split_action_, &QAction::triggered, this,
            [this] { set_view_mode(ViewMode::Split); });

    mode_quad_action_ = view_menu->addAction("&Quad (4 views)");
    mode_quad_action_->setCheckable(true);
    mode_quad_action_->setShortcut(QKeySequence("F7"));
    connect(mode_quad_action_, &QAction::triggered, this,
            [this] { set_view_mode(ViewMode::Quad); });

    auto* mode_group = new QActionGroup(this);
    mode_group->setExclusive(true);
    mode_group->addAction(mode_plan_action_);
    mode_group->addAction(mode_front_action_);
    mode_group->addAction(mode_side_action_);
    mode_group->addAction(mode_iso_action_);
    mode_group->addAction(mode_split_action_);
    mode_group->addAction(mode_quad_action_);
}

void MainWindow::build_toolbar() {
    auto* tools = addToolBar("Tools");
    tools->setMovable(false);

    auto* group = new QActionGroup(this);
    group->setExclusive(true);

    select_action_ = tools->addAction("Select");
    select_action_->setCheckable(true);
    select_action_->setShortcut(QKeySequence("S"));
    group->addAction(select_action_);
    connect(select_action_, &QAction::triggered, this, &MainWindow::activate_select_tool);

    wall_action_ = tools->addAction("Wall");
    wall_action_->setCheckable(true);
    wall_action_->setShortcut(QKeySequence("W"));
    group->addAction(wall_action_);
    connect(wall_action_, &QAction::triggered, this, &MainWindow::activate_wall_tool);

    box_action_ = tools->addAction("Box");
    box_action_->setCheckable(true);
    box_action_->setShortcut(QKeySequence("B"));
    group->addAction(box_action_);
    connect(box_action_, &QAction::triggered, this, &MainWindow::activate_box_tool);

    cylinder_action_ = tools->addAction("Cylinder");
    cylinder_action_->setCheckable(true);
    cylinder_action_->setShortcut(QKeySequence("C"));
    group->addAction(cylinder_action_);
    connect(cylinder_action_, &QAction::triggered, this, &MainWindow::activate_cylinder_tool);

    door_action_ = tools->addAction("Door");
    door_action_->setCheckable(true);
    door_action_->setShortcut(QKeySequence("D"));
    group->addAction(door_action_);
    connect(door_action_, &QAction::triggered, this, &MainWindow::activate_door_tool);

    window_action_ = tools->addAction("Window");
    window_action_->setCheckable(true);
    window_action_->setShortcut(QKeySequence("N"));
    group->addAction(window_action_);
    connect(window_action_, &QAction::triggered, this, &MainWindow::activate_window_tool);

    slab_action_ = tools->addAction("Slab");
    slab_action_->setCheckable(true);
    slab_action_->setShortcut(QKeySequence("L"));
    group->addAction(slab_action_);
    connect(slab_action_, &QAction::triggered, this, &MainWindow::activate_slab_tool);

    curve_action_ = tools->addAction("NURBS Curve");
    curve_action_->setCheckable(true);
    curve_action_->setShortcut(QKeySequence("U"));
    group->addAction(curve_action_);
    connect(curve_action_, &QAction::triggered, this, &MainWindow::activate_curve_tool);

    surface_action_ = tools->addAction("NURBS Surface");
    surface_action_->setCheckable(true);
    surface_action_->setShortcut(QKeySequence("F"));
    group->addAction(surface_action_);
    connect(surface_action_, &QAction::triggered, this, &MainWindow::activate_surface_tool);

    dimension_action_ = tools->addAction("Dimension");
    dimension_action_->setCheckable(true);
    dimension_action_->setShortcut(QKeySequence("M"));
    group->addAction(dimension_action_);
    connect(dimension_action_, &QAction::triggered, this, &MainWindow::activate_dimension_tool);

    tools->addSeparator();
    tools->addAction(undo_action_);
    tools->addAction(redo_action_);

    auto* view_bar = addToolBar("View");
    view_bar->setMovable(false);
    view_bar->addAction(mode_plan_action_);
    view_bar->addAction(mode_front_action_);
    view_bar->addAction(mode_side_action_);
    view_bar->addAction(mode_iso_action_);
    view_bar->addAction(mode_split_action_);
    view_bar->addAction(mode_quad_action_);

    auto* cam_bar = addToolBar("Camera");
    cam_bar->setMovable(false);
    auto add_preset = [&](const QString& label, cadino::ui::Viewport3D::CameraPreset p,
                          const QKeySequence& key = {}) {
        auto* act = cam_bar->addAction(label);
        if (!key.isEmpty()) act->setShortcut(key);
        connect(act, &QAction::triggered, this, [this, p, label] {
            viewport_3d_->set_preset(p);
            statusBar()->showMessage(QString("Camera: %1").arg(label));
        });
    };
    add_preset("Iso", cadino::ui::Viewport3D::CameraPreset::Iso, QKeySequence("1"));
    add_preset("Top", cadino::ui::Viewport3D::CameraPreset::Top, QKeySequence("2"));
    add_preset("Front", cadino::ui::Viewport3D::CameraPreset::Front, QKeySequence("3"));
    add_preset("Back", cadino::ui::Viewport3D::CameraPreset::Back, QKeySequence("4"));
    add_preset("Left", cadino::ui::Viewport3D::CameraPreset::Left, QKeySequence("5"));
    add_preset("Right", cadino::ui::Viewport3D::CameraPreset::Right, QKeySequence("6"));
}

void MainWindow::activate_select_tool() {
    plan_view_->set_tool(std::make_unique<cadino::ui::SelectTool>());
    if (select_action_) select_action_->setChecked(true);
    statusBar()->showMessage("Select tool — click a wall to select, drag to move");
}

void MainWindow::activate_wall_tool() {
    plan_view_->set_tool(std::make_unique<cadino::ui::WallTool>());
    if (wall_action_) wall_action_->setChecked(true);
    statusBar()->showMessage("Wall tool — click two points to draw a wall (Esc to cancel)");
}

void MainWindow::activate_box_tool() {
    plan_view_->set_tool(std::make_unique<cadino::ui::BoxTool>());
    if (box_action_) box_action_->setChecked(true);
    statusBar()->showMessage("Box tool — click two opposite corners (default height 750mm). Esc to cancel.");
}

void MainWindow::activate_cylinder_tool() {
    plan_view_->set_tool(std::make_unique<cadino::ui::CylinderTool>());
    if (cylinder_action_) cylinder_action_->setChecked(true);
    statusBar()->showMessage("Cylinder tool — click center then radius (default height 750mm). Esc to cancel.");
}

void MainWindow::activate_door_tool() {
    plan_view_->set_tool(std::make_unique<cadino::ui::DoorTool>(false));
    if (door_action_) door_action_->setChecked(true);
    statusBar()->showMessage("Door tool — click on a wall to place a door");
}

void MainWindow::activate_window_tool() {
    plan_view_->set_tool(std::make_unique<cadino::ui::DoorTool>(true));
    if (window_action_) window_action_->setChecked(true);
    statusBar()->showMessage("Window tool — click on a wall to place a window");
}

void MainWindow::activate_curve_tool() {
    plan_view_->set_tool(std::make_unique<cadino::ui::NurbsCurveTool>());
    if (curve_action_) curve_action_->setChecked(true);
    statusBar()->showMessage(
        "NURBS curve — left-click adds control points, right-click to finish (Esc cancels)");
}

void MainWindow::activate_surface_tool() {
    plan_view_->set_tool(std::make_unique<cadino::ui::NurbsSurfaceTool>());
    if (surface_action_) surface_action_->setChecked(true);
    statusBar()->showMessage(
        "NURBS surface — click two corners to drop a 4x4 grid at 1500 mm (Esc cancels)");
}

void MainWindow::activate_slab_tool() {
    plan_view_->set_tool(std::make_unique<cadino::ui::SlabTool>());
    if (slab_action_) slab_action_->setChecked(true);
    statusBar()->showMessage("Slab tool — click two opposite corners to create a floor slab");
}

void MainWindow::activate_dimension_tool() {
    plan_view_->set_tool(std::make_unique<cadino::ui::DimensionTool>());
    if (dimension_action_) dimension_action_->setChecked(true);
    statusBar()->showMessage(
        "Dimension tool — click two endpoints, then click to position the dimension line (Esc cancels)");
}

void MainWindow::set_view_mode(ViewMode mode) {
    view_mode_ = mode;
    plan_view_->setVisible(false);
    viewport_3d_->setVisible(false);
    front_view_->setVisible(false);
    side_view_->setVisible(false);

    switch (mode) {
        case ViewMode::PlanOnly:
            plan_view_->setVisible(true);
            if (mode_plan_action_) mode_plan_action_->setChecked(true);
            statusBar()->showMessage("View: Plan (Top) only");
            break;
        case ViewMode::FrontOnly:
            front_view_->setVisible(true);
            if (mode_front_action_) mode_front_action_->setChecked(true);
            statusBar()->showMessage("View: Front elevation only");
            break;
        case ViewMode::SideOnly:
            side_view_->setVisible(true);
            if (mode_side_action_) mode_side_action_->setChecked(true);
            statusBar()->showMessage("View: Side elevation only");
            break;
        case ViewMode::IsoOnly:
            viewport_3d_->setVisible(true);
            if (mode_iso_action_) mode_iso_action_->setChecked(true);
            statusBar()->showMessage("View: 3D only");
            break;
        case ViewMode::Split:
            plan_view_->setVisible(true);
            viewport_3d_->setVisible(true);
            if (mode_split_action_) mode_split_action_->setChecked(true);
            statusBar()->showMessage("View: Split (Plan + 3D)");
            break;
        case ViewMode::Quad:
            plan_view_->setVisible(true);
            viewport_3d_->setVisible(true);
            front_view_->setVisible(true);
            side_view_->setVisible(true);
            if (mode_quad_action_) mode_quad_action_->setChecked(true);
            statusBar()->showMessage("View: Quad — Plan / 3D / Front / Side");
            break;
    }

    top_row_->setVisible(plan_view_->isVisible() || viewport_3d_->isVisible());
    bottom_row_->setVisible(front_view_->isVisible() || side_view_->isVisible());

    // Read sizes from the splitter (the only widget whose extent doesn't
    // collapse to 0 when its sub-rows hide). Reading top_row_->width() etc.
    // is unsafe across consecutive mode switches because the previous
    // setSizes call's layout pass may still be pending.
    const int splitter_h = std::max(splitter_->height(), 600);
    const int row_w = std::max(splitter_->width(), 800);
    if (mode == ViewMode::Quad) {
        splitter_->setSizes({splitter_h / 2, splitter_h / 2});
        top_row_->setSizes({row_w / 2, row_w / 2});
        bottom_row_->setSizes({row_w / 2, row_w / 2});
    } else if (mode == ViewMode::Split) {
        splitter_->setSizes({splitter_h, 0});
        top_row_->setSizes({row_w / 2, row_w / 2});
    } else if (mode == ViewMode::PlanOnly) {
        splitter_->setSizes({splitter_h, 0});
        top_row_->setSizes({row_w, 0});
    } else if (mode == ViewMode::IsoOnly) {
        splitter_->setSizes({splitter_h, 0});
        top_row_->setSizes({0, row_w});
    } else if (mode == ViewMode::FrontOnly) {
        splitter_->setSizes({0, splitter_h});
        bottom_row_->setSizes({row_w, 0});
    } else if (mode == ViewMode::SideOnly) {
        splitter_->setSizes({0, splitter_h});
        bottom_row_->setSizes({0, row_w});
    }
}

void MainWindow::update_undo_redo_actions() {
    undo_action_->setEnabled(stack_.can_undo());
    redo_action_->setEnabled(stack_.can_redo());
}

void MainWindow::new_document() {
    if (QMessageBox::question(this, "New",
            "Discard the current document and start a new one?") != QMessageBox::Yes) {
        return;
    }
    document_ = cadino::core::Document{};
    stack_.clear();
    plan_view_->clear_selection();
    plan_view_->update();
    viewport_3d_->update();
    update_undo_redo_actions();
    current_file_path_.clear();
    setWindowTitle("Cadino");
    statusBar()->showMessage("New document");
}

void MainWindow::open_document() {
    const QString path = QFileDialog::getOpenFileName(
        this, "Open Cadino document", current_file_path_,
        "Cadino documents (*.cadino);;JSON (*.json);;All files (*)");
    if (path.isEmpty()) return;

    QString error;
    if (!cadino::ui::load_document_from_file(document_, path, &error)) {
        QMessageBox::warning(this, "Open failed", error);
        return;
    }
    stack_.clear();
    plan_view_->clear_selection();
    plan_view_->update();
    viewport_3d_->update();
    update_undo_redo_actions();
    current_file_path_ = path;
    setWindowTitle(QString("Cadino — %1").arg(QFileInfo(path).fileName()));
    statusBar()->showMessage(QString("Opened %1").arg(path));
}

bool MainWindow::save_document() {
    if (current_file_path_.isEmpty()) return save_document_as();
    QString error;
    if (!cadino::ui::save_document_to_file(document_, current_file_path_, &error)) {
        QMessageBox::warning(this, "Save failed", error);
        return false;
    }
    statusBar()->showMessage(QString("Saved %1").arg(current_file_path_));
    return true;
}

void MainWindow::export_dxf() {
    QString path = QFileDialog::getSaveFileName(
        this, "Export DXF", current_file_path_, "AutoCAD DXF (*.dxf)");
    if (path.isEmpty()) return;
    if (!path.contains('.')) path += ".dxf";

    QString error;
    if (!cadino::ui::export_document_as_dxf(document_, path, &error)) {
        QMessageBox::warning(this, "Export failed", error);
        return;
    }
    statusBar()->showMessage(QString("Exported DXF to %1").arg(path));
}

void MainWindow::export_obj() {
    QString path = QFileDialog::getSaveFileName(
        this, "Export OBJ", current_file_path_, "Wavefront OBJ (*.obj)");
    if (path.isEmpty()) return;
    if (!path.endsWith(".obj", Qt::CaseInsensitive)) path += ".obj";

    QString error;
    if (!cadino::ui::export_as_obj(document_, path, &error)) {
        QMessageBox::warning(this, "Export failed", error);
        return;
    }
    statusBar()->showMessage(QString("Exported OBJ to %1").arg(path));
}

void MainWindow::export_stl() {
    QString path = QFileDialog::getSaveFileName(
        this, "Export STL", current_file_path_, "STL (*.stl)");
    if (path.isEmpty()) return;
    if (!path.endsWith(".stl", Qt::CaseInsensitive)) path += ".stl";

    QString error;
    if (!cadino::ui::export_as_stl(document_, path, &error)) {
        QMessageBox::warning(this, "Export failed", error);
        return;
    }
    statusBar()->showMessage(QString("Exported STL to %1").arg(path));
}

void MainWindow::export_elevation_dxf(int plane_index) {
    cadino::ui::ElevationPlane plane = cadino::ui::ElevationPlane::Front;
    QString name = "front";
    switch (plane_index) {
        case 0: plane = cadino::ui::ElevationPlane::Front; name = "front"; break;
        case 1: plane = cadino::ui::ElevationPlane::Back;  name = "back";  break;
        case 2: plane = cadino::ui::ElevationPlane::Left;  name = "left";  break;
        case 3: plane = cadino::ui::ElevationPlane::Right; name = "right"; break;
    }
    QString suggested = current_file_path_;
    if (!suggested.isEmpty()) {
        const QFileInfo fi(suggested);
        suggested = fi.absolutePath() + "/" + fi.baseName() + "-" + name + ".dxf";
    }
    QString path = QFileDialog::getSaveFileName(
        this, QString("Export %1 elevation").arg(name), suggested, "AutoCAD DXF (*.dxf)");
    if (path.isEmpty()) return;
    if (!path.endsWith(".dxf", Qt::CaseInsensitive)) path += ".dxf";

    QString error;
    if (!cadino::ui::export_elevation_as_dxf(document_, plane, path, &error)) {
        QMessageBox::warning(this, "Export failed", error);
        return;
    }
    statusBar()->showMessage(QString("Exported %1 elevation to %2").arg(name, path));
}

void MainWindow::import_3dm() {
#ifdef CADINO_HAS_OPENNURBS
    const QString path = QFileDialog::getOpenFileName(
        this, "Import Rhino 3DM", current_file_path_,
        "Rhino 3DM (*.3dm);;All files (*)");
    if (path.isEmpty()) return;

    std::string error;
    if (!cadino::io::import_3dm(document_, path.toStdString(), &error)) {
        QMessageBox::warning(this, "Import failed",
                             QString::fromStdString(error.empty() ? "Unknown error" : error));
        return;
    }
    stack_.clear();
    plan_view_->clear_selection();
    plan_view_->update();
    viewport_3d_->update();
    update_undo_redo_actions();
    statusBar()->showMessage(QString("Imported %1").arg(path));
#else
    QMessageBox::information(this, "openNURBS disabled",
                             "Cadino was built without openNURBS support.");
#endif
}

void MainWindow::export_3dm() {
#ifdef CADINO_HAS_OPENNURBS
    QString path = QFileDialog::getSaveFileName(
        this, "Export Rhino 3DM", current_file_path_, "Rhino 3DM (*.3dm)");
    if (path.isEmpty()) return;
    if (!path.endsWith(".3dm", Qt::CaseInsensitive)) path += ".3dm";

    std::string error;
    if (!cadino::io::export_3dm(document_, path.toStdString(), &error)) {
        QMessageBox::warning(this, "Export failed",
                             QString::fromStdString(error.empty() ? "Unknown error" : error));
        return;
    }
    statusBar()->showMessage(QString("Exported 3DM to %1").arg(path));
#else
    QMessageBox::information(this, "openNURBS disabled",
                             "Cadino was built without openNURBS support.");
#endif
}

bool MainWindow::save_document_as() {
    QString path = QFileDialog::getSaveFileName(
        this, "Save Cadino document", current_file_path_,
        "Cadino documents (*.cadino);;JSON (*.json)");
    if (path.isEmpty()) return false;
    if (!path.contains('.')) path += ".cadino";

    QString error;
    if (!cadino::ui::save_document_to_file(document_, path, &error)) {
        QMessageBox::warning(this, "Save failed", error);
        return false;
    }
    current_file_path_ = path;
    setWindowTitle(QString("Cadino — %1").arg(QFileInfo(path).fileName()));
    statusBar()->showMessage(QString("Saved %1").arg(path));
    return true;
}

void MainWindow::group_selected() {
    const auto selections = plan_view_->selections();
    if (selections.size() < 2) {
        statusBar()->showMessage("Select at least 2 entities to group");
        return;
    }
    const auto new_group = cadino::core::next_entity_id();
    for (const auto& sel : selections) {
        if (sel.kind == cadino::ui::SelectKind::Wall) {
            if (auto* w = document_.find_wall(sel.id)) w->group_id = new_group;
        } else if (sel.kind == cadino::ui::SelectKind::Box) {
            if (auto* b = document_.find_box(sel.id)) b->group_id = new_group;
        } else if (sel.kind == cadino::ui::SelectKind::Cylinder) {
            if (auto* c = document_.find_cylinder(sel.id)) c->group_id = new_group;
        }
    }
    plan_view_->notify_document_modified();
    statusBar()->showMessage(QString("Grouped %1 entities").arg(selections.size()));
}

namespace {

void remove_entity(cadino::core::Document& doc, const cadino::ui::Selection& sel) {
    if (sel.kind == cadino::ui::SelectKind::Box) doc.remove_box(sel.id);
    else if (sel.kind == cadino::ui::SelectKind::Cylinder) doc.remove_cylinder(sel.id);
}

}  // namespace

void MainWindow::subtract_selected() {
    const auto selections = plan_view_->selections();
    if (selections.size() != 2) {
        statusBar()->showMessage("Subtract: select exactly two entities (target first, tool second)");
        return;
    }
    const auto target = selections[0];
    const auto tool = selections[1];
    auto result = cadino::ui::subtract_entities(document_, target, tool);
    if (!result) {
        statusBar()->showMessage("Subtract failed (only Box/Cylinder targets currently supported)");
        return;
    }
    const auto id = document_.add_mesh(std::move(*result));
    remove_entity(document_, target);
    remove_entity(document_, tool);

    plan_view_->clear_selection();
    plan_view_->notify_document_modified();
    statusBar()->showMessage(QString("Created mesh from boolean cut (id=%1)").arg(id.value));
}

void MainWindow::union_selected() {
    const auto selections = plan_view_->selections();
    if (selections.size() != 2) {
        statusBar()->showMessage("Union: select exactly two entities (Box or Cylinder)");
        return;
    }
    auto result = cadino::ui::union_entities(document_, selections[0], selections[1]);
    if (!result) {
        statusBar()->showMessage("Union failed (only Box/Cylinder currently supported)");
        return;
    }
    const auto id = document_.add_mesh(std::move(*result));
    remove_entity(document_, selections[0]);
    remove_entity(document_, selections[1]);

    plan_view_->clear_selection();
    plan_view_->notify_document_modified();
    statusBar()->showMessage(QString("Created mesh from boolean union (id=%1)").arg(id.value));
}

void MainWindow::make_block_from_selected() {
    const auto selections = plan_view_->selections();
    if (selections.empty()) {
        statusBar()->showMessage("Make Block: select boxes / cylinders first");
        return;
    }

    std::vector<cadino::core::Box> boxes;
    std::vector<cadino::core::Cylinder> cylinders;
    double sx = 0.0, sy = 0.0;
    int count = 0;
    for (const auto& sel : selections) {
        if (sel.kind == cadino::ui::SelectKind::Box) {
            if (const auto* b = document_.find_box(sel.id)) {
                boxes.push_back(*b);
                sx += b->position.x();
                sy += b->position.y();
                ++count;
            }
        } else if (sel.kind == cadino::ui::SelectKind::Cylinder) {
            if (const auto* c = document_.find_cylinder(sel.id)) {
                cylinders.push_back(*c);
                sx += c->position.x();
                sy += c->position.y();
                ++count;
            }
        }
    }
    if (count == 0) {
        statusBar()->showMessage("Make Block: select boxes or cylinders only");
        return;
    }

    const double cx = sx / count;
    const double cy = sy / count;
    cadino::core::Block block;
    block.name = "Block";
    block.position = {cx, cy};
    block.rotation_z = 0.0;
    block.base_z = 0.0;
    for (auto b : boxes) {
        b.id = {};
        b.position = {b.position.x() - cx, b.position.y() - cy};
        block.boxes.push_back(std::move(b));
    }
    for (auto c : cylinders) {
        c.id = {};
        c.position = {c.position.x() - cx, c.position.y() - cy};
        block.cylinders.push_back(std::move(c));
    }

    // Remove originals + add block as one composite command sequence.
    for (const auto& sel : selections) {
        if (sel.kind == cadino::ui::SelectKind::Box) {
            stack_.execute(std::make_unique<cadino::core::RemoveBoxCommand>(sel.id));
        } else if (sel.kind == cadino::ui::SelectKind::Cylinder) {
            stack_.execute(std::make_unique<cadino::core::RemoveCylinderCommand>(sel.id));
        }
    }
    stack_.execute(std::make_unique<cadino::core::AddBlockCommand>(std::move(block)));

    plan_view_->clear_selection();
    plan_view_->notify_document_modified();
    statusBar()->showMessage(QString("Packed %1 entities into a Block").arg(count));
}

void MainWindow::define_block_from_selected() {
    const auto selections = plan_view_->selections();
    if (selections.empty()) {
        statusBar()->showMessage("Define Block: select boxes / cylinders first");
        return;
    }

    std::vector<cadino::core::Box> boxes;
    std::vector<cadino::core::Cylinder> cylinders;
    double sx = 0.0, sy = 0.0;
    int count = 0;
    for (const auto& sel : selections) {
        if (sel.kind == cadino::ui::SelectKind::Box) {
            if (const auto* b = document_.find_box(sel.id)) {
                boxes.push_back(*b);
                sx += b->position.x();
                sy += b->position.y();
                ++count;
            }
        } else if (sel.kind == cadino::ui::SelectKind::Cylinder) {
            if (const auto* c = document_.find_cylinder(sel.id)) {
                cylinders.push_back(*c);
                sx += c->position.x();
                sy += c->position.y();
                ++count;
            }
        }
    }
    if (count == 0) {
        statusBar()->showMessage("Define Block: select boxes or cylinders only");
        return;
    }

    bool ok = false;
    QString name = QInputDialog::getText(this, "Define Block",
                                         "Definition name:", QLineEdit::Normal,
                                         "Furniture", &ok);
    if (!ok || name.isEmpty()) return;

    const double cx = sx / count;
    const double cy = sy / count;
    cadino::core::BlockDefinition def;
    def.name = name.toStdString();
    for (auto b : boxes) {
        b.id = {};
        b.position = {b.position.x() - cx, b.position.y() - cy};
        def.boxes.push_back(std::move(b));
    }
    for (auto c : cylinders) {
        c.id = {};
        c.position = {c.position.x() - cx, c.position.y() - cy};
        def.cylinders.push_back(std::move(c));
    }

    auto def_cmd = std::make_unique<cadino::core::AddBlockDefinitionCommand>(std::move(def));
    const auto* def_cmd_ptr = def_cmd.get();
    stack_.execute(std::move(def_cmd));
    const auto def_id = def_cmd_ptr->entity_id();

    // Remove originals and drop a first instance at the centroid.
    for (const auto& sel : selections) {
        if (sel.kind == cadino::ui::SelectKind::Box) {
            stack_.execute(std::make_unique<cadino::core::RemoveBoxCommand>(sel.id));
        } else if (sel.kind == cadino::ui::SelectKind::Cylinder) {
            stack_.execute(std::make_unique<cadino::core::RemoveCylinderCommand>(sel.id));
        }
    }

    cadino::core::BlockInstance inst;
    inst.definition_id = def_id;
    inst.position = {cx, cy};
    stack_.execute(std::make_unique<cadino::core::AddBlockInstanceCommand>(std::move(inst)));

    plan_view_->clear_selection();
    plan_view_->notify_document_modified();
    statusBar()->showMessage(QString("Defined Block '%1' (%2 children) and placed one instance")
                                 .arg(name).arg(count));
}

void MainWindow::insert_block_instance() {
    if (document_.block_defs().empty()) {
        statusBar()->showMessage("No Block definitions yet — use 'Define Block from Selection' first");
        return;
    }
    QStringList items;
    std::vector<cadino::core::EntityId> ids;
    for (const auto& [id, def] : document_.block_defs()) {
        items << QString::fromStdString(def.name) + QString(" (id=%1)").arg(id.value);
        ids.push_back(id);
    }
    bool ok = false;
    const QString picked = QInputDialog::getItem(this, "Insert Block Instance",
                                                 "Definition:", items, 0, false, &ok);
    if (!ok) return;
    const int idx = items.indexOf(picked);
    if (idx < 0) return;

    cadino::core::BlockInstance inst;
    inst.definition_id = ids[static_cast<std::size_t>(idx)];
    inst.position = {0.0, 0.0};
    stack_.execute(std::make_unique<cadino::core::AddBlockInstanceCommand>(std::move(inst)));
    plan_view_->notify_document_modified();
    statusBar()->showMessage("Inserted block instance at origin — drag with the Select tool");
}

void MainWindow::explode_selected_block() {
    const auto selections = plan_view_->selections();
    if (selections.empty()) {
        statusBar()->showMessage("Explode: select a Block first");
        return;
    }
    int exploded = 0;
    for (const auto& sel : selections) {
        if (sel.kind != cadino::ui::SelectKind::Block) continue;
        const auto* bl = document_.find_block(sel.id);
        if (!bl) continue;
        // Snapshot children in world coordinates.
        std::vector<cadino::core::Box> world_boxes;
        std::vector<cadino::core::Cylinder> world_cyls;
        world_boxes.reserve(bl->boxes.size());
        world_cyls.reserve(bl->cylinders.size());
        for (const auto& local_b : bl->boxes) {
            auto wb = bl->world_box(local_b);
            wb.id = {};
            world_boxes.push_back(std::move(wb));
        }
        for (const auto& local_c : bl->cylinders) {
            auto wc = bl->world_cylinder(local_c);
            wc.id = {};
            world_cyls.push_back(std::move(wc));
        }
        stack_.execute(std::make_unique<cadino::core::RemoveBlockCommand>(sel.id));
        for (auto& wb : world_boxes) {
            stack_.execute(std::make_unique<cadino::core::AddBoxCommand>(std::move(wb)));
        }
        for (auto& wc : world_cyls) {
            stack_.execute(std::make_unique<cadino::core::AddCylinderCommand>(std::move(wc)));
        }
        ++exploded;
    }
    if (exploded == 0) {
        statusBar()->showMessage("Explode: no Block in selection");
        return;
    }
    plan_view_->clear_selection();
    plan_view_->notify_document_modified();
    statusBar()->showMessage(QString("Exploded %1 block(s)").arg(exploded));
}

void MainWindow::intersect_selected() {
    const auto selections = plan_view_->selections();
    if (selections.size() != 2) {
        statusBar()->showMessage("Intersect: select exactly two entities (Box or Cylinder)");
        return;
    }
    auto result = cadino::ui::intersect_entities(document_, selections[0], selections[1]);
    if (!result) {
        statusBar()->showMessage("Intersect failed (only Box/Cylinder currently supported)");
        return;
    }
    const auto id = document_.add_mesh(std::move(*result));
    remove_entity(document_, selections[0]);
    remove_entity(document_, selections[1]);

    plan_view_->clear_selection();
    plan_view_->notify_document_modified();
    statusBar()->showMessage(QString("Created mesh from boolean intersection (id=%1)").arg(id.value));
}

void MainWindow::ungroup_selected() {
    const auto selections = plan_view_->selections();
    if (selections.empty()) return;
    for (const auto& sel : selections) {
        if (sel.kind == cadino::ui::SelectKind::Wall) {
            if (auto* w = document_.find_wall(sel.id)) w->group_id = {};
        } else if (sel.kind == cadino::ui::SelectKind::Box) {
            if (auto* b = document_.find_box(sel.id)) b->group_id = {};
        } else if (sel.kind == cadino::ui::SelectKind::Cylinder) {
            if (auto* c = document_.find_cylinder(sel.id)) c->group_id = {};
        }
    }
    plan_view_->notify_document_modified();
    statusBar()->showMessage("Ungrouped selection");
}

void MainWindow::delete_selected() {
    const auto selections = plan_view_->selections();
    if (selections.empty()) return;

    for (const auto& sel : selections) {
        switch (sel.kind) {
            case cadino::ui::SelectKind::Wall:
                stack_.execute(std::make_unique<cadino::core::RemoveWallCommand>(sel.id));
                break;
            case cadino::ui::SelectKind::Box:
                stack_.execute(std::make_unique<cadino::core::RemoveBoxCommand>(sel.id));
                break;
            case cadino::ui::SelectKind::Cylinder:
                stack_.execute(std::make_unique<cadino::core::RemoveCylinderCommand>(sel.id));
                break;
            case cadino::ui::SelectKind::NurbsCurve:
                stack_.execute(std::make_unique<cadino::core::RemoveNurbsCurveCommand>(sel.id));
                break;
            case cadino::ui::SelectKind::Block:
                stack_.execute(std::make_unique<cadino::core::RemoveBlockCommand>(sel.id));
                break;
            case cadino::ui::SelectKind::NurbsSurface:
                stack_.execute(std::make_unique<cadino::core::RemoveNurbsSurfaceCommand>(sel.id));
                break;
            case cadino::ui::SelectKind::BlockInstance:
                stack_.execute(std::make_unique<cadino::core::RemoveBlockInstanceCommand>(sel.id));
                break;
            case cadino::ui::SelectKind::Dimension:
                stack_.execute(std::make_unique<cadino::core::RemoveDimensionCommand>(sel.id));
                break;
            case cadino::ui::SelectKind::None:
                break;
        }
    }
    plan_view_->clear_selection();
    plan_view_->notify_document_modified();
}

}  // namespace cadino::app
