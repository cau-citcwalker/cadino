#include "MainWindow.hpp"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QInputDialog>
#include <QLineEdit>
#include <QKeySequence>
#include <QLabel>
#include <QMenuBar>
#include <QMenu>
#include <QMessageBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QPushButton>
#include <QSaveFile>
#include <QSpinBox>
#include <QSplitter>
#include <QTableWidget>
#include <QTextStream>
#include <QStatusBar>
#include <QTimer>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWidget>

#include <spdlog/spdlog.h>

#include "BooleanOps.hpp"
#include "BoxTool.hpp"
#include "Alignment.hpp"
#include "Array.hpp"
#include "Clipboard.hpp"
#include <QSettings>

#include "MaterialLibrary.hpp"
#include "DimensionTool.hpp"
#include "LayerPanel.hpp"
#include "MirrorTool.hpp"
#include "OffsetTool.hpp"
#include "PolarArrayTool.hpp"
#include "AngularDimensionTool.hpp"
#include "LeaderTool.hpp"
#include "RadialDimensionTool.hpp"
#include "TextTool.hpp"
#include "TrimExtendTool.hpp"

#include "command/AngularDimensionCommands.hpp"
#include "command/LeaderCommands.hpp"
#include "command/RadialDimensionCommands.hpp"
#include "command/TextAnnotationCommands.hpp"
#include "CylinderTool.hpp"
#include "DocumentIO.hpp"
#include "DoorTool.hpp"
#include "DxfExporter.hpp"
#include "IfcExporter.hpp"
#include "MeshExport.hpp"
#include "PlanView.hpp"
#include "PropertiesPanel.hpp"
#include "SelectTool.hpp"
#include "NurbsCurveTool.hpp"
#include "NurbsSurfaceTool.hpp"
#include "SlabTool.hpp"
#include "LineTool.hpp"
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

    // Apply user snap preferences before the first paint.
    {
        QSettings s("Cadino", "Cadino");
        auto& snap = plan_view_->snap_engine();
        snap.set_grid_step(s.value("snap/grid_step", snap.grid_step()).toDouble());
        snap.set_grid_enabled(s.value("snap/grid", snap.grid_enabled()).toBool());
        snap.set_endpoint_enabled(s.value("snap/endpoint", snap.endpoint_enabled()).toBool());
        snap.set_midpoint_enabled(s.value("snap/midpoint", snap.midpoint_enabled()).toBool());
        snap.set_corner_enabled(s.value("snap/corner", snap.corner_enabled()).toBool());
        snap.set_center_enabled(s.value("snap/center", snap.center_enabled()).toBool());
        snap.set_intersection_enabled(s.value("snap/intersection", snap.intersection_enabled()).toBool());
        snap.set_perpendicular_enabled(s.value("snap/perpendicular", snap.perpendicular_enabled()).toBool());
        snap.set_nearest_enabled(s.value("snap/nearest", snap.nearest_enabled()).toBool());
    }

    // Defer the initial mode switch until after Qt finishes the first window
    // layout. Calling set_view_mode (which toggles widget visibility) during
    // construction races with the QSplitter measure pass and leaves both
    // viewports stuck at 0 size.
    QTimer::singleShot(0, this, [this] { set_view_mode(ViewMode::Split); });

    ortho_status_ = new QLabel(this);
    ortho_status_->setStyleSheet("QLabel { color: #d97706; font-weight: bold; padding: 0 6px; }");
    statusBar()->addPermanentWidget(ortho_status_);
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
    front_view_ = new cadino::ui::PlanView(document_, stack_, bottom_row_);
    side_view_ = new cadino::ui::PlanView(document_, stack_, bottom_row_);

    front_view_->set_plane(cadino::ui::DrawPlane::Front);
    side_view_->set_plane(cadino::ui::DrawPlane::Right);

    auto refresh_children = [this] {
        viewport_3d_->refresh();
        front_view_->update();
        side_view_->update();
    };
    connect(plan_view_, &cadino::ui::PlanView::document_modified, this,
            [this, refresh_children] {
        refresh_children();
        update_undo_redo_actions();
    });
    connect(plan_view_, &cadino::ui::PlanView::selection_changed, this,
            refresh_children);

    // Elevation views editing writes into the same doc; propagate back to the
    // plan view + 3D viewport so all four stay in sync.
    auto broadcast_from = [this](cadino::ui::PlanView* source) {
        source->update();
        plan_view_->update();
        viewport_3d_->refresh();
        if (source != front_view_ && front_view_) front_view_->update();
        if (source != side_view_ && side_view_)   side_view_->update();
        update_undo_redo_actions();
    };
    connect(front_view_, &cadino::ui::PlanView::document_modified, this,
            [broadcast_from, this] { broadcast_from(front_view_); });
    connect(side_view_, &cadino::ui::PlanView::document_modified, this,
            [broadcast_from, this] { broadcast_from(side_view_); });

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

    recent_menu_ = file_menu->addMenu("Open &Recent");
    rebuild_recent_menu();

    file_menu->addSeparator();
    auto* prefs_a = file_menu->addAction("Pre&ferences...");
    prefs_a->setShortcut(QKeySequence("Ctrl+,"));
    connect(prefs_a, &QAction::triggered, this, &MainWindow::show_preferences_dialog);

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

    auto* export_ifc_a = file_menu->addAction("Export &IFC...");
    connect(export_ifc_a, &QAction::triggered, this, &MainWindow::export_ifc);

    file_menu->addSeparator();
    auto* schedule_a = file_menu->addAction("&Schedule...");
    schedule_a->setShortcut(QKeySequence("Ctrl+Shift+S"));
    connect(schedule_a, &QAction::triggered, this, &MainWindow::show_schedule);

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
    auto* copy_a = edit_menu->addAction("&Copy");
    copy_a->setShortcut(QKeySequence::Copy);
    connect(copy_a, &QAction::triggered, this, &MainWindow::copy_selected);

    auto* paste_a = edit_menu->addAction("&Paste");
    paste_a->setShortcut(QKeySequence::Paste);
    connect(paste_a, &QAction::triggered, this, &MainWindow::paste_clipboard);

    auto* duplicate_a = edit_menu->addAction("Du&plicate");
    duplicate_a->setShortcut(QKeySequence("Ctrl+D"));
    connect(duplicate_a, &QAction::triggered, this, &MainWindow::duplicate_selected);

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

    edit_menu->addSeparator();
    auto* mirror_a = edit_menu->addAction("&Mirror");
    mirror_a->setShortcut(QKeySequence("Ctrl+M"));
    connect(mirror_a, &QAction::triggered, this, &MainWindow::activate_mirror_tool);

    auto* rect_array_a = edit_menu->addAction("Re&ctangular Array...");
    rect_array_a->setShortcut(QKeySequence("Ctrl+Alt+R"));
    connect(rect_array_a, &QAction::triggered, this, &MainWindow::rectangular_array_dialog);

    auto* polar_array_a = edit_menu->addAction("Po&lar Array...");
    polar_array_a->setShortcut(QKeySequence("Ctrl+Alt+P"));
    connect(polar_array_a, &QAction::triggered, this, &MainWindow::polar_array_dialog);

    auto* offset_a = edit_menu->addAction("&Offset Wall...");
    offset_a->setShortcut(QKeySequence("Ctrl+Alt+O"));
    connect(offset_a, &QAction::triggered, this, &MainWindow::activate_offset_tool);

    auto* trim_a = edit_menu->addAction("&Trim / Extend Wall");
    trim_a->setShortcut(QKeySequence("Ctrl+Alt+T"));
    connect(trim_a, &QAction::triggered, this, &MainWindow::activate_trim_extend_tool);

    materials_menu_ = menuBar()->addMenu("Mate&rials");
    rebuild_materials_menu();

    auto* align_menu = menuBar()->addMenu("Ali&gn");
    auto add_align = [&](const QString& label, cadino::ui::AlignMode mode,
                         const QKeySequence& key = {}) {
        auto* a = align_menu->addAction(label);
        if (!key.isEmpty()) a->setShortcut(key);
        connect(a, &QAction::triggered, this, [this, mode] {
            const int n = cadino::ui::apply_alignment(
                document_, stack_, plan_view_->selections(), mode);
            if (n > 0) {
                plan_view_->notify_document_modified();
                statusBar()->showMessage(QString::number(n) + " entities aligned");
            }
        });
    };
    add_align("Align &Left", cadino::ui::AlignMode::Left, QKeySequence("Ctrl+Shift+L"));
    add_align("Align &Right", cadino::ui::AlignMode::Right, QKeySequence("Ctrl+Shift+R"));
    add_align("Align &Top", cadino::ui::AlignMode::Top, QKeySequence("Ctrl+Shift+T"));
    add_align("Align &Bottom", cadino::ui::AlignMode::Bottom, QKeySequence("Ctrl+Shift+B"));
    add_align("Center &Horizontal", cadino::ui::AlignMode::CenterH, QKeySequence("Ctrl+Shift+H"));
    add_align("Center &Vertical", cadino::ui::AlignMode::CenterV, QKeySequence("Ctrl+Shift+V"));
    align_menu->addSeparator();
    add_align("Distribute Hori&zontally", cadino::ui::AlignMode::DistributeH,
              QKeySequence("Ctrl+Alt+H"));
    add_align("&Distribute Vertically", cadino::ui::AlignMode::DistributeV,
              QKeySequence("Ctrl+Alt+V"));

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

    view_menu->addSeparator();
    ortho_action_ = view_menu->addAction("&Ortho Mode");
    ortho_action_->setCheckable(true);
    ortho_action_->setShortcut(QKeySequence("F8"));
    connect(ortho_action_, &QAction::triggered, this, [this](bool on) {
        plan_view_->set_ortho_enabled(on);
        if (ortho_status_) ortho_status_->setText(on ? "ORTHO" : "");
        statusBar()->showMessage(on ? "Ortho ON — direction locked to X/Y axes"
                                    : "Ortho OFF");
        plan_view_->update();
    });

    view_menu->addSeparator();
    auto* sun_a = view_menu->addAction("Sun &Position...");
    sun_a->setShortcut(QKeySequence("Ctrl+Shift+P"));
    connect(sun_a, &QAction::triggered, this, &MainWindow::show_sun_dialog);

    auto* section_menu = view_menu->addMenu("Section");
    auto* section_off = section_menu->addAction("Off");
    connect(section_off, &QAction::triggered, this,
            [this] { set_section_axis(-1, 0.0); });
    auto* section_x = section_menu->addAction("Slice along X...");
    connect(section_x, &QAction::triggered, this, [this] {
        bool ok = false;
        const double pos = QInputDialog::getDouble(
            this, "Section X", "X position (mm):", 2000.0,
            -1'000'000.0, 1'000'000.0, 0, &ok);
        if (ok) set_section_axis(0, pos);
    });
    auto* section_y = section_menu->addAction("Slice along Y...");
    connect(section_y, &QAction::triggered, this, [this] {
        bool ok = false;
        const double pos = QInputDialog::getDouble(
            this, "Section Y", "Y position (mm):", 1500.0,
            -1'000'000.0, 1'000'000.0, 0, &ok);
        if (ok) set_section_axis(1, pos);
    });
    auto* section_z = section_menu->addAction("Slice along Z...");
    connect(section_z, &QAction::triggered, this, [this] {
        bool ok = false;
        const double pos = QInputDialog::getDouble(
            this, "Section Z", "Z position (mm):", 1200.0,
            -1'000'000.0, 1'000'000.0, 0, &ok);
        if (ok) set_section_axis(2, pos);
    });

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
    slab_action_->setShortcut(QKeySequence("H"));
    group->addAction(slab_action_);
    connect(slab_action_, &QAction::triggered, this, &MainWindow::activate_slab_tool);

    line_action_ = tools->addAction("Line");
    line_action_->setCheckable(true);
    line_action_->setShortcut(QKeySequence("L"));
    group->addAction(line_action_);
    connect(line_action_, &QAction::triggered, this, &MainWindow::activate_line_tool);

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

    auto* text_action = tools->addAction("Text");
    text_action->setCheckable(true);
    text_action->setShortcut(QKeySequence("T"));
    group->addAction(text_action);
    connect(text_action, &QAction::triggered, this, &MainWindow::activate_text_tool);

    auto* leader_action = tools->addAction("Leader");
    leader_action->setCheckable(true);
    leader_action->setShortcut(QKeySequence("Shift+L"));
    group->addAction(leader_action);
    connect(leader_action, &QAction::triggered, this, &MainWindow::activate_leader_tool);

    auto* ang_action = tools->addAction("Angle");
    ang_action->setCheckable(true);
    ang_action->setShortcut(QKeySequence("Shift+A"));
    group->addAction(ang_action);
    connect(ang_action, &QAction::triggered, this, &MainWindow::activate_angular_dim_tool);

    auto* rad_action = tools->addAction("Radius");
    rad_action->setCheckable(true);
    rad_action->setShortcut(QKeySequence("Shift+R"));
    group->addAction(rad_action);
    connect(rad_action, &QAction::triggered, this, &MainWindow::activate_radial_dim_tool);

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
    // Prefixed so digits stay free for tools that accept numeric input
    // (Line tool's direct distance entry, dimension override, etc.).
    add_preset("Iso", cadino::ui::Viewport3D::CameraPreset::Iso, QKeySequence("Ctrl+1"));
    add_preset("Top", cadino::ui::Viewport3D::CameraPreset::Top, QKeySequence("Ctrl+2"));
    add_preset("Front", cadino::ui::Viewport3D::CameraPreset::Front, QKeySequence("Ctrl+3"));
    add_preset("Back", cadino::ui::Viewport3D::CameraPreset::Back, QKeySequence("Ctrl+4"));
    add_preset("Left", cadino::ui::Viewport3D::CameraPreset::Left, QKeySequence("Ctrl+5"));
    add_preset("Right", cadino::ui::Viewport3D::CameraPreset::Right, QKeySequence("Ctrl+6"));
}

void MainWindow::activate_select_tool() {
    set_tool_all_views<cadino::ui::SelectTool>();
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

void MainWindow::activate_line_tool() {
    set_tool_all_views<cadino::ui::LineTool>();
    if (line_action_) line_action_->setChecked(true);
    plan_view_->setFocus();
    statusBar()->showMessage(
        "Line — click points in any 2D view; type a number then Enter for "
        "direct distance; Shift=ortho; right-click or Enter to finish (Esc cancels)");
}

void MainWindow::activate_dimension_tool() {
    plan_view_->set_tool(std::make_unique<cadino::ui::DimensionTool>());
    if (dimension_action_) dimension_action_->setChecked(true);
    statusBar()->showMessage(
        "Dimension tool — click two endpoints, then click to position the dimension line (Esc cancels)");
}

void MainWindow::activate_text_tool() {
    bool ok = false;
    const QString text = QInputDialog::getText(
        this, "Text", "Text:", QLineEdit::Normal, "Text", &ok);
    if (!ok || text.isEmpty()) return;
    plan_view_->set_tool(std::make_unique<cadino::ui::TextTool>(text));
    statusBar()->showMessage("Text — click to place the label (Esc cancels)");
}

void MainWindow::activate_leader_tool() {
    bool ok = false;
    const QString text = QInputDialog::getText(
        this, "Leader", "Callout text:", QLineEdit::Normal, "Note", &ok);
    if (!ok || text.isEmpty()) return;
    plan_view_->set_tool(std::make_unique<cadino::ui::LeaderTool>(text));
    statusBar()->showMessage(
        "Leader — click the anchor, then click where the text goes (Esc cancels)");
}

void MainWindow::activate_angular_dim_tool() {
    plan_view_->set_tool(std::make_unique<cadino::ui::AngularDimensionTool>());
    statusBar()->showMessage(
        "Angular dim — click the vertex, then a point on each arm (Esc cancels)");
}

void MainWindow::activate_radial_dim_tool() {
    const auto modifiers = QApplication::keyboardModifiers();
    const bool diameter = (modifiers & Qt::ShiftModifier) != 0;
    plan_view_->set_tool(std::make_unique<cadino::ui::RadialDimensionTool>(diameter));
    statusBar()->showMessage(
        diameter
            ? "Diameter — click a cylinder, then click where the label goes"
            : "Radius — click a cylinder, then click where the label goes (Shift+click toolbar for Ø)");
}

void MainWindow::activate_mirror_tool() {
    if (plan_view_->selections().empty()) {
        statusBar()->showMessage("Mirror — select entities first, then click two points for the axis");
    } else {
        statusBar()->showMessage(
            "Mirror — click two points to define the axis (Shift on the 2nd click = move-mirror, Esc cancels)");
    }
    plan_view_->set_tool(std::make_unique<cadino::ui::MirrorTool>());
}

void MainWindow::rectangular_array_dialog() {
    if (plan_view_->selections().empty()) {
        statusBar()->showMessage("Rectangular array — select entities first");
        return;
    }

    QDialog dlg(this);
    dlg.setWindowTitle("Rectangular Array");
    auto* form = new QFormLayout(&dlg);

    auto* rows = new QSpinBox(&dlg); rows->setRange(1, 200); rows->setValue(2);
    auto* cols = new QSpinBox(&dlg); cols->setRange(1, 200); cols->setValue(3);
    auto* dx = new QDoubleSpinBox(&dlg);
    dx->setRange(-1'000'000.0, 1'000'000.0); dx->setDecimals(1);
    dx->setSingleStep(100.0); dx->setSuffix(" mm"); dx->setValue(1000.0);
    auto* dy = new QDoubleSpinBox(&dlg);
    dy->setRange(-1'000'000.0, 1'000'000.0); dy->setDecimals(1);
    dy->setSingleStep(100.0); dy->setSuffix(" mm"); dy->setValue(1000.0);

    form->addRow("Rows (Y)", rows);
    form->addRow("Cols (X)", cols);
    form->addRow("X spacing", dx);
    form->addRow("Y spacing", dy);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted) return;

    const int n = cadino::ui::rectangular_array(
        document_, stack_, plan_view_->selections(),
        rows->value(), cols->value(), dx->value(), dy->value());
    plan_view_->notify_document_modified();
    statusBar()->showMessage(QString("Rectangular array created %1 copies").arg(n));
}

void MainWindow::activate_offset_tool() {
    bool ok = false;
    const double dist = QInputDialog::getDouble(
        this, "Offset Wall", "Distance (mm):", 500.0,
        -100'000.0, 100'000.0, 1, &ok);
    if (!ok) return;
    plan_view_->set_tool(std::make_unique<cadino::ui::OffsetTool>(dist));
    statusBar()->showMessage(
        "Offset — click a wall, then click the side where the parallel copy goes (Esc cancels)");
}

void MainWindow::activate_trim_extend_tool() {
    plan_view_->set_tool(std::make_unique<cadino::ui::TrimExtendTool>());
    statusBar()->showMessage(
        "Trim/Extend — click the boundary wall, then click the wall to modify near the endpoint that should snap to the intersection");
}

void MainWindow::polar_array_dialog() {
    if (plan_view_->selections().empty()) {
        statusBar()->showMessage("Polar array — select entities first");
        return;
    }

    QDialog dlg(this);
    dlg.setWindowTitle("Polar Array");
    auto* form = new QFormLayout(&dlg);

    auto* count = new QSpinBox(&dlg); count->setRange(2, 360); count->setValue(6);
    auto* sweep = new QDoubleSpinBox(&dlg);
    sweep->setRange(-360.0, 360.0); sweep->setDecimals(1);
    sweep->setSingleStep(15.0); sweep->setSuffix(" deg"); sweep->setValue(360.0);

    form->addRow("Total count", count);
    form->addRow("Sweep angle", sweep);
    form->addRow(new QLabel("After OK: click in the plan view to set the rotation centre."));

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted) return;

    const double sweep_rad = sweep->value() * 3.14159265358979323846 / 180.0;
    plan_view_->set_tool(std::make_unique<cadino::ui::PolarArrayTool>(
        count->value(), sweep_rad));
    statusBar()->showMessage("Polar array — click the rotation centre in the plan view");
}

void MainWindow::set_view_mode(ViewMode mode) {
    view_mode_ = mode;

    // Decide intent first, then apply visibility top-down. Reading
    // `child->isVisible()` after setVisible(true) lies when the parent row
    // is still hidden — it returned false and the row never re-appeared,
    // so any switch out of PlanOnly hid every panel.
    bool want_plan = false, want_3d = false, want_front = false, want_side = false;
    switch (mode) {
        case ViewMode::PlanOnly:  want_plan = true; break;
        case ViewMode::FrontOnly: want_front = true; break;
        case ViewMode::SideOnly:  want_side = true; break;
        case ViewMode::IsoOnly:   want_3d = true; break;
        case ViewMode::Split:     want_plan = true; want_3d = true; break;
        case ViewMode::Quad:      want_plan = want_3d = want_front = want_side = true; break;
    }

    // Show / hide the row containers first so their children's setVisible
    // calls actually take effect on the next layout.
    top_row_->setVisible(want_plan || want_3d);
    bottom_row_->setVisible(want_front || want_side);
    plan_view_->setVisible(want_plan);
    viewport_3d_->setVisible(want_3d);
    front_view_->setVisible(want_front);
    side_view_->setVisible(want_side);

    if (mode_plan_action_)  mode_plan_action_->setChecked(mode == ViewMode::PlanOnly);
    if (mode_front_action_) mode_front_action_->setChecked(mode == ViewMode::FrontOnly);
    if (mode_side_action_)  mode_side_action_->setChecked(mode == ViewMode::SideOnly);
    if (mode_iso_action_)   mode_iso_action_->setChecked(mode == ViewMode::IsoOnly);
    if (mode_split_action_) mode_split_action_->setChecked(mode == ViewMode::Split);
    if (mode_quad_action_)  mode_quad_action_->setChecked(mode == ViewMode::Quad);
    switch (mode) {
        case ViewMode::PlanOnly:  statusBar()->showMessage("View: Plan (Top) only"); break;
        case ViewMode::FrontOnly: statusBar()->showMessage("View: Front elevation only"); break;
        case ViewMode::SideOnly:  statusBar()->showMessage("View: Side elevation only"); break;
        case ViewMode::IsoOnly:   statusBar()->showMessage("View: 3D only"); break;
        case ViewMode::Split:     statusBar()->showMessage("View: Split (Plan + 3D)"); break;
        case ViewMode::Quad:      statusBar()->showMessage("View: Quad — Plan / 3D / Front / Side"); break;
    }

    // Defer setSizes until after the visibility flips have settled. Calling
    // setSizes while a child's show/hide is still queued has crashed Qt in
    // this codebase before (see project_qsplitter_pitfall).
    QTimer::singleShot(0, this, [this, mode] {
        if (!splitter_ || !top_row_ || !bottom_row_) return;
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
    });
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
    open_path(path);
}

void MainWindow::open_path(const QString& path) {
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
    add_recent(path);
}

void MainWindow::add_recent(const QString& path) {
    QSettings s("Cadino", "Cadino");
    QStringList list = s.value("recent_files").toStringList();
    list.removeAll(path);
    list.prepend(path);
    while (list.size() > 10) list.removeLast();
    s.setValue("recent_files", list);
    rebuild_recent_menu();
}

void MainWindow::rebuild_recent_menu() {
    if (!recent_menu_) return;
    recent_menu_->clear();
    QSettings s("Cadino", "Cadino");
    const QStringList list = s.value("recent_files").toStringList();
    if (list.isEmpty()) {
        auto* a = recent_menu_->addAction("(empty)");
        a->setEnabled(false);
        return;
    }
    for (const auto& path : list) {
        const QString label = QFileInfo(path).fileName() + "  —  " + path;
        auto* a = recent_menu_->addAction(label);
        connect(a, &QAction::triggered, this, [this, path] { open_path(path); });
    }
    recent_menu_->addSeparator();
    auto* clear_a = recent_menu_->addAction("Clear");
    connect(clear_a, &QAction::triggered, this, [this] {
        QSettings s("Cadino", "Cadino");
        s.remove("recent_files");
        rebuild_recent_menu();
    });
}

bool MainWindow::save_document() {
    if (current_file_path_.isEmpty()) return save_document_as();
    QString error;
    if (!cadino::ui::save_document_to_file(document_, current_file_path_, &error)) {
        QMessageBox::warning(this, "Save failed", error);
        return false;
    }
    statusBar()->showMessage(QString("Saved %1").arg(current_file_path_));
    add_recent(current_file_path_);
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

namespace {
struct ScheduleRow {
    QString type;
    QString detail;
    QString quantity;
};

double slab_area(const cadino::core::Slab& s) {
    if (s.outline.size() < 3) return 0.0;
    double area = 0.0;
    for (std::size_t i = 0; i < s.outline.size(); ++i) {
        const auto& p = s.outline[i];
        const auto& q = s.outline[(i + 1) % s.outline.size()];
        area += p.x() * q.y() - q.x() * p.y();
    }
    return std::abs(area) * 0.5;
}
}  // namespace

void MainWindow::rebuild_materials_menu() {
    if (!materials_menu_) return;
    materials_menu_->clear();

    auto* save_a = materials_menu_->addAction("Save Selection as Preset...");
    connect(save_a, &QAction::triggered, this, &MainWindow::save_selection_material);

    materials_menu_->addSeparator();
    const auto& presets = cadino::ui::MaterialLibrary::instance().presets();
    if (presets.empty()) {
        auto* empty = materials_menu_->addAction("(no presets yet)");
        empty->setEnabled(false);
        return;
    }
    auto* apply_menu = materials_menu_->addMenu("Apply to Selection");
    for (const auto& p : presets) {
        const std::string name = p.name;
        auto* a = apply_menu->addAction(QString::fromStdString(name));
        connect(a, &QAction::triggered, this, [this, name] { apply_material(name); });
    }
    auto* delete_menu = materials_menu_->addMenu("Delete Preset");
    for (const auto& p : presets) {
        const std::string name = p.name;
        auto* a = delete_menu->addAction(QString::fromStdString(name));
        connect(a, &QAction::triggered, this, [this, name] {
            cadino::ui::MaterialLibrary::instance().remove(name);
            rebuild_materials_menu();
            statusBar()->showMessage(
                QString("Deleted preset \"%1\"").arg(QString::fromStdString(name)));
        });
    }
}

void MainWindow::save_selection_material() {
    if (plan_view_->selections().empty()) {
        statusBar()->showMessage("Material — select an entity first");
        return;
    }
    bool ok = false;
    const QString name = QInputDialog::getText(
        this, "Save material preset", "Preset name:",
        QLineEdit::Normal, "Material", &ok);
    if (!ok || name.isEmpty()) return;
    if (cadino::ui::MaterialLibrary::instance().capture_from_selection(
            document_, plan_view_->selections(), name.toStdString())) {
        rebuild_materials_menu();
        statusBar()->showMessage(QString("Saved preset \"%1\"").arg(name));
    } else {
        statusBar()->showMessage("Selection has no material-bearing entity");
    }
}

void MainWindow::apply_material(const std::string& name) {
    const int n = cadino::ui::MaterialLibrary::instance().apply_to_selection(
        document_, stack_, plan_view_->selections(), name);
    plan_view_->notify_document_modified();
    statusBar()->showMessage(
        QString("Applied preset \"%1\" to %2 entities")
            .arg(QString::fromStdString(name)).arg(n));
}

void MainWindow::show_preferences_dialog() {
    QDialog dlg(this);
    dlg.setWindowTitle("Preferences");
    dlg.resize(380, 420);
    auto* form = new QFormLayout(&dlg);

    QSettings s("Cadino", "Cadino");
    auto& snap = plan_view_->snap_engine();

    auto* grid_step = new QDoubleSpinBox(&dlg);
    grid_step->setRange(1.0, 100000.0);
    grid_step->setDecimals(1);
    grid_step->setSuffix(" mm");
    grid_step->setValue(snap.grid_step());

    auto* cb_grid = new QCheckBox("Grid", &dlg); cb_grid->setChecked(snap.grid_enabled());
    auto* cb_endpoint = new QCheckBox("Endpoint", &dlg); cb_endpoint->setChecked(snap.endpoint_enabled());
    auto* cb_midpoint = new QCheckBox("Midpoint", &dlg); cb_midpoint->setChecked(snap.midpoint_enabled());
    auto* cb_corner = new QCheckBox("Corner", &dlg); cb_corner->setChecked(snap.corner_enabled());
    auto* cb_center = new QCheckBox("Center", &dlg); cb_center->setChecked(snap.center_enabled());
    auto* cb_intersection = new QCheckBox("Intersection", &dlg); cb_intersection->setChecked(snap.intersection_enabled());
    auto* cb_perpendicular = new QCheckBox("Perpendicular", &dlg); cb_perpendicular->setChecked(snap.perpendicular_enabled());
    auto* cb_nearest = new QCheckBox("Nearest", &dlg); cb_nearest->setChecked(snap.nearest_enabled());

    auto* units = new QComboBox(&dlg);
    units->addItems({"mm", "cm", "m", "inch"});
    units->setCurrentText(s.value("display_units", "mm").toString());

    form->addRow("Display units", units);
    form->addRow("Grid step", grid_step);
    form->addRow(new QLabel("Snap kinds:", &dlg));
    form->addRow(cb_grid);
    form->addRow(cb_endpoint);
    form->addRow(cb_midpoint);
    form->addRow(cb_corner);
    form->addRow(cb_center);
    form->addRow(cb_intersection);
    form->addRow(cb_perpendicular);
    form->addRow(cb_nearest);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted) return;

    snap.set_grid_step(grid_step->value());
    snap.set_grid_enabled(cb_grid->isChecked());
    snap.set_endpoint_enabled(cb_endpoint->isChecked());
    snap.set_midpoint_enabled(cb_midpoint->isChecked());
    snap.set_corner_enabled(cb_corner->isChecked());
    snap.set_center_enabled(cb_center->isChecked());
    snap.set_intersection_enabled(cb_intersection->isChecked());
    snap.set_perpendicular_enabled(cb_perpendicular->isChecked());
    snap.set_nearest_enabled(cb_nearest->isChecked());

    s.setValue("display_units", units->currentText());
    s.setValue("snap/grid_step", grid_step->value());
    s.setValue("snap/grid", cb_grid->isChecked());
    s.setValue("snap/endpoint", cb_endpoint->isChecked());
    s.setValue("snap/midpoint", cb_midpoint->isChecked());
    s.setValue("snap/corner", cb_corner->isChecked());
    s.setValue("snap/center", cb_center->isChecked());
    s.setValue("snap/intersection", cb_intersection->isChecked());
    s.setValue("snap/perpendicular", cb_perpendicular->isChecked());
    s.setValue("snap/nearest", cb_nearest->isChecked());

    statusBar()->showMessage("Preferences saved");
}

void MainWindow::show_sun_dialog() {
    QDialog dlg(this);
    dlg.setWindowTitle("Sun Position");
    auto* form = new QFormLayout(&dlg);

    auto* az = new QDoubleSpinBox(&dlg);
    az->setRange(-360.0, 360.0);
    az->setSingleStep(5.0);
    az->setSuffix(" deg");
    az->setValue(viewport_3d_->sun_azimuth());

    auto* al = new QDoubleSpinBox(&dlg);
    al->setRange(0.0, 90.0);
    al->setSingleStep(5.0);
    al->setSuffix(" deg");
    al->setValue(viewport_3d_->sun_altitude());

    form->addRow("Azimuth", az);
    form->addRow("Altitude", al);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted) return;
    const float a = static_cast<float>(az->value());
    const float t = static_cast<float>(al->value());
    viewport_3d_->set_sun(a, t);
    statusBar()->showMessage(
        QString("Sun: azimuth %1°, altitude %2°").arg(a, 0, 'f', 1).arg(t, 0, 'f', 1));
}

void MainWindow::set_section_axis(int axis, double position) {
    if (axis < 0) {
        viewport_3d_->set_section(false, {0, 0, 1}, {0, 0, 0});
        statusBar()->showMessage("Section: off");
        return;
    }
    QVector3D normal;
    QVector3D point;
    switch (axis) {
        case 0: normal = {1, 0, 0}; point = {static_cast<float>(position), 0, 0}; break;
        case 1: normal = {0, 1, 0}; point = {0, static_cast<float>(position), 0}; break;
        case 2: normal = {0, 0, 1}; point = {0, 0, static_cast<float>(position)}; break;
        default: return;
    }
    viewport_3d_->set_section(true, normal, point);
    statusBar()->showMessage(
        QString("Section: axis %1 at %2 mm")
            .arg(axis == 0 ? "X" : axis == 1 ? "Y" : "Z")
            .arg(position, 0, 'f', 0));
}

void MainWindow::show_schedule() {
    std::vector<ScheduleRow> rows;
    for (const auto& [id, w] : document_.walls()) {
        const double len = (w.end - w.start).norm();
        rows.push_back({"Wall",
            QString("len=%1mm thk=%2mm h=%3mm").arg(len, 0, 'f', 1)
                .arg(w.thickness, 0, 'f', 0).arg(w.height, 0, 'f', 0),
            "1"});
    }
    for (const auto& [id, d] : document_.doors()) {
        rows.push_back({"Door",
            QString("w=%1mm h=%2mm").arg(d.width, 0, 'f', 0).arg(d.height, 0, 'f', 0),
            "1"});
    }
    for (const auto& [id, w] : document_.windows()) {
        rows.push_back({"Window",
            QString("w=%1mm h=%2mm sill=%3mm")
                .arg(w.width, 0, 'f', 0).arg(w.height, 0, 'f', 0).arg(w.sill_height, 0, 'f', 0),
            "1"});
    }
    for (const auto& [id, b] : document_.boxes()) {
        rows.push_back({"Box",
            QString("%1x%2 h=%3mm").arg(b.size_xy.x(), 0, 'f', 0)
                .arg(b.size_xy.y(), 0, 'f', 0).arg(b.height, 0, 'f', 0),
            "1"});
    }
    for (const auto& [id, c] : document_.cylinders()) {
        rows.push_back({"Cylinder",
            QString("Ø %1mm h=%2mm").arg(c.radius * 2.0, 0, 'f', 0)
                .arg(c.height, 0, 'f', 0),
            "1"});
    }
    for (const auto& [id, s] : document_.slabs()) {
        rows.push_back({"Slab",
            QString("area=%1 m^2 t=%2mm")
                .arg(slab_area(s) / 1.0e6, 0, 'f', 2).arg(s.thickness, 0, 'f', 0),
            "1"});
    }
    for (const auto& [id, i] : document_.block_instances()) {
        QString name = "(missing)";
        if (const auto* def = document_.find_block_def(i.definition_id)) {
            name = QString::fromStdString(def->name);
        }
        rows.push_back({"Block instance",
            QString("def=\"%1\" pos=(%2,%3)").arg(name)
                .arg(i.position.x(), 0, 'f', 0).arg(i.position.y(), 0, 'f', 0),
            "1"});
    }

    QDialog dlg(this);
    dlg.setWindowTitle("Schedule");
    dlg.resize(720, 480);
    auto* outer = new QVBoxLayout(&dlg);

    auto* totals = new QLabel(&dlg);
    totals->setText(QString(
        "Walls: %1, Doors: %2, Windows: %3, Boxes: %4, Cylinders: %5, "
        "Slabs: %6, Block instances: %7")
        .arg(document_.walls().size())
        .arg(document_.doors().size())
        .arg(document_.windows().size())
        .arg(document_.boxes().size())
        .arg(document_.cylinders().size())
        .arg(document_.slabs().size())
        .arg(document_.block_instances().size()));
    outer->addWidget(totals);

    auto* table = new QTableWidget(&dlg);
    table->setColumnCount(3);
    table->setHorizontalHeaderLabels({"Type", "Details", "Qty"});
    table->setRowCount(static_cast<int>(rows.size()));
    for (int r = 0; r < static_cast<int>(rows.size()); ++r) {
        table->setItem(r, 0, new QTableWidgetItem(rows[r].type));
        table->setItem(r, 1, new QTableWidgetItem(rows[r].detail));
        table->setItem(r, 2, new QTableWidgetItem(rows[r].quantity));
    }
    table->horizontalHeader()->setStretchLastSection(true);
    outer->addWidget(table);

    auto* btns = new QHBoxLayout();
    auto* csv = new QPushButton("Export CSV...", &dlg);
    auto* close = new QPushButton("Close", &dlg);
    btns->addWidget(csv);
    btns->addStretch();
    btns->addWidget(close);
    outer->addLayout(btns);

    connect(close, &QPushButton::clicked, &dlg, &QDialog::accept);
    connect(csv, &QPushButton::clicked, this, [this, &rows]() {
        QString path = QFileDialog::getSaveFileName(
            this, "Export schedule CSV", current_file_path_,
            "CSV (*.csv)");
        if (path.isEmpty()) return;
        if (!path.endsWith(".csv", Qt::CaseInsensitive)) path += ".csv";
        QSaveFile f(path);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QMessageBox::warning(this, "Export failed", f.errorString());
            return;
        }
        QTextStream out(&f);
        out << "Type,Details,Qty\n";
        for (const auto& r : rows) {
            QString d = r.detail;
            d.replace('"', "\"\"");
            out << r.type << ",\"" << d << "\"," << r.quantity << "\n";
        }
        f.commit();
        statusBar()->showMessage(QString("Schedule exported to %1").arg(path));
    });
    dlg.exec();
}

void MainWindow::export_ifc() {
    QString path = QFileDialog::getSaveFileName(
        this, "Export IFC", current_file_path_, "IFC (*.ifc)");
    if (path.isEmpty()) return;
    if (!path.endsWith(".ifc", Qt::CaseInsensitive)) path += ".ifc";

    QString error;
    if (!cadino::ui::export_document_as_ifc(document_, path, &error)) {
        QMessageBox::warning(this, "Export failed", error);
        return;
    }
    statusBar()->showMessage(QString("Exported IFC to %1").arg(path));
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

void MainWindow::copy_selected() {
    cadino::ui::Clipboard::instance().put(document_, plan_view_->selections());
    statusBar()->showMessage(
        QString("Copied %1 entities").arg(plan_view_->selections().size()));
}

void MainWindow::paste_clipboard() {
    auto& clip = cadino::ui::Clipboard::instance();
    if (clip.empty()) {
        statusBar()->showMessage("Clipboard is empty");
        return;
    }
    const auto added = clip.paste(document_, stack_, {300.0, 300.0});
    plan_view_->set_selections(added);
    plan_view_->notify_document_modified();
    statusBar()->showMessage(QString("Pasted %1 entities").arg(added.size()));
}

void MainWindow::duplicate_selected() {
    auto& clip = cadino::ui::Clipboard::instance();
    clip.put(document_, plan_view_->selections());
    if (clip.empty()) return;
    const auto added = clip.paste(document_, stack_, {300.0, 300.0});
    plan_view_->set_selections(added);
    plan_view_->notify_document_modified();
    statusBar()->showMessage(QString("Duplicated %1 entities").arg(added.size()));
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
            case cadino::ui::SelectKind::Text:
                stack_.execute(std::make_unique<cadino::core::RemoveTextAnnotationCommand>(sel.id));
                break;
            case cadino::ui::SelectKind::Leader:
                stack_.execute(std::make_unique<cadino::core::RemoveLeaderCommand>(sel.id));
                break;
            case cadino::ui::SelectKind::AngularDimension:
                stack_.execute(std::make_unique<cadino::core::RemoveAngularDimensionCommand>(sel.id));
                break;
            case cadino::ui::SelectKind::RadialDimension:
                stack_.execute(std::make_unique<cadino::core::RemoveRadialDimensionCommand>(sel.id));
                break;
            case cadino::ui::SelectKind::None:
                break;
        }
    }
    plan_view_->clear_selection();
    plan_view_->notify_document_modified();
}

}  // namespace cadino::app
