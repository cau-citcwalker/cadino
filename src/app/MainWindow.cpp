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

#include "BoxTool.hpp"
#include "CylinderTool.hpp"
#include "DocumentIO.hpp"
#include "PlanView.hpp"
#include "PropertiesPanel.hpp"
#include "SelectTool.hpp"
#include "Viewport3D.hpp"
#include "WallTool.hpp"

#include "command/BoxCommands.hpp"
#include "command/CylinderCommands.hpp"
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

    connect(plan_view_, &cadino::ui::PlanView::selection_changed, properties_,
            &cadino::ui::PropertiesPanel::set_selection);
    connect(plan_view_, &cadino::ui::PlanView::document_modified, properties_,
            &cadino::ui::PropertiesPanel::refresh);

    activate_select_tool();
    set_view_mode(ViewMode::Split);
    statusBar()->showMessage("Ready");
    spdlog::info("MainWindow ready");
}

MainWindow::~MainWindow() = default;

void MainWindow::build_central_widget() {
    splitter_ = new QSplitter(Qt::Horizontal, this);
    splitter_->setHandleWidth(4);
    splitter_->setChildrenCollapsible(false);

    plan_view_ = new cadino::ui::PlanView(document_, stack_, splitter_);
    auto* viewport = new cadino::ui::Viewport3D(document_, splitter_);
    viewport_3d_ = viewport;

    connect(plan_view_, &cadino::ui::PlanView::document_modified, this, [this, viewport] {
        viewport->refresh();
        update_undo_redo_actions();
    });

    splitter_->addWidget(plan_view_);
    splitter_->addWidget(viewport_3d_);
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

    auto* view_menu = menuBar()->addMenu("&View");
    mode_plan_action_ = view_menu->addAction("&Plan (2D)");
    mode_plan_action_->setCheckable(true);
    mode_plan_action_->setShortcut(QKeySequence("F2"));
    connect(mode_plan_action_, &QAction::triggered, this,
            [this] { set_view_mode(ViewMode::PlanOnly); });

    mode_viewport_action_ = view_menu->addAction("&3D");
    mode_viewport_action_->setCheckable(true);
    mode_viewport_action_->setShortcut(QKeySequence("F3"));
    connect(mode_viewport_action_, &QAction::triggered, this,
            [this] { set_view_mode(ViewMode::ViewportOnly); });

    mode_split_action_ = view_menu->addAction("&Split");
    mode_split_action_->setCheckable(true);
    mode_split_action_->setShortcut(QKeySequence("F4"));
    connect(mode_split_action_, &QAction::triggered, this,
            [this] { set_view_mode(ViewMode::Split); });

    auto* mode_group = new QActionGroup(this);
    mode_group->setExclusive(true);
    mode_group->addAction(mode_plan_action_);
    mode_group->addAction(mode_viewport_action_);
    mode_group->addAction(mode_split_action_);
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

    tools->addSeparator();
    tools->addAction(undo_action_);
    tools->addAction(redo_action_);

    auto* view_bar = addToolBar("View");
    view_bar->setMovable(false);
    view_bar->addAction(mode_plan_action_);
    view_bar->addAction(mode_viewport_action_);
    view_bar->addAction(mode_split_action_);
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

void MainWindow::set_view_mode(ViewMode mode) {
    view_mode_ = mode;
    switch (mode) {
        case ViewMode::PlanOnly:
            plan_view_->setVisible(true);
            viewport_3d_->setVisible(false);
            if (mode_plan_action_) mode_plan_action_->setChecked(true);
            statusBar()->showMessage("View: 2D Plan only");
            break;
        case ViewMode::ViewportOnly:
            plan_view_->setVisible(false);
            viewport_3d_->setVisible(true);
            if (mode_viewport_action_) mode_viewport_action_->setChecked(true);
            statusBar()->showMessage("View: 3D only");
            break;
        case ViewMode::Split:
            plan_view_->setVisible(true);
            viewport_3d_->setVisible(true);
            if (mode_split_action_) mode_split_action_->setChecked(true);
            splitter_->setSizes({splitter_->width() / 2, splitter_->width() / 2});
            statusBar()->showMessage("View: Split (2D + 3D)");
            break;
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

void MainWindow::delete_selected() {
    const auto& sel = plan_view_->selection();
    if (!sel.valid()) return;

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
            return;
    }
    plan_view_->clear_selection();
    plan_view_->notify_document_modified();
}

}  // namespace cadino::app
