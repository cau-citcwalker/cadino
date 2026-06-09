#include "MainWindow.hpp"

#include <QAction>
#include <QActionGroup>
#include <QDockWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QKeySequence>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QSplitter>
#include <QStatusBar>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWidget>

#include <spdlog/spdlog.h>

#include "BooleanOps.hpp"
#include "BoxTool.hpp"
#include "CylinderTool.hpp"
#include "DocumentIO.hpp"
#include "DoorTool.hpp"
#include "DxfExporter.hpp"
#include "PlanView.hpp"
#include "PropertiesPanel.hpp"
#include "SelectTool.hpp"
#include "SlabTool.hpp"
#include "Viewport3D.hpp"
#include "WallTool.hpp"
#include <QKeySequence>

#include "command/BoxCommands.hpp"
#include "command/CylinderCommands.hpp"

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

    connect(plan_view_, &cadino::ui::PlanView::selection_changed, this, [this] {
        properties_->set_selection(plan_view_->primary_selection());
    });
    connect(plan_view_, &cadino::ui::PlanView::document_modified, properties_,
            &cadino::ui::PropertiesPanel::refresh);

    activate_select_tool();
    set_view_mode(ViewMode::Split);
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
    top_row_->setSizes({600, 600});

    bottom_row_->addWidget(front_view_);
    bottom_row_->addWidget(side_view_);
    bottom_row_->setStretchFactor(0, 1);
    bottom_row_->setStretchFactor(1, 1);
    bottom_row_->setSizes({600, 600});

    splitter_->addWidget(top_row_);
    splitter_->addWidget(bottom_row_);
    splitter_->setStretchFactor(0, 1);
    splitter_->setStretchFactor(1, 1);
    splitter_->setSizes({400, 400});

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
    auto* import_3dm_a = file_menu->addAction("&Import Rhino 3DM...");
    connect(import_3dm_a, &QAction::triggered, this, &MainWindow::import_3dm);

    auto* export_3dm_a = file_menu->addAction("E&xport Rhino 3DM...");
    connect(export_3dm_a, &QAction::triggered, this, &MainWindow::export_3dm);

    auto* export_dxf_a = file_menu->addAction("Export &DXF...");
    connect(export_dxf_a, &QAction::triggered, this, &MainWindow::export_dxf);

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

void MainWindow::activate_slab_tool() {
    plan_view_->set_tool(std::make_unique<cadino::ui::SlabTool>());
    if (slab_action_) slab_action_->setChecked(true);
    statusBar()->showMessage("Slab tool — click two opposite corners to create a floor slab");
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

    if (mode == ViewMode::Quad) {
        splitter_->setSizes({400, 400});
        top_row_->setSizes({600, 600});
        bottom_row_->setSizes({600, 600});
    } else if (mode == ViewMode::Split) {
        splitter_->setSizes({800, 0});
        top_row_->setSizes({600, 600});
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
    if (target.kind == cadino::ui::SelectKind::Box) document_.remove_box(target.id);
    else if (target.kind == cadino::ui::SelectKind::Cylinder) document_.remove_cylinder(target.id);
    if (tool.kind == cadino::ui::SelectKind::Box) document_.remove_box(tool.id);
    else if (tool.kind == cadino::ui::SelectKind::Cylinder) document_.remove_cylinder(tool.id);

    plan_view_->clear_selection();
    plan_view_->notify_document_modified();
    statusBar()->showMessage(QString("Created mesh from boolean cut (id=%1)").arg(id.value));
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
            case cadino::ui::SelectKind::None:
                break;
        }
    }
    plan_view_->clear_selection();
    plan_view_->notify_document_modified();
}

}  // namespace cadino::app
