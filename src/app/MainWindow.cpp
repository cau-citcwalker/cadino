#include "MainWindow.hpp"

#include <QAction>
#include <QActionGroup>
#include <QKeySequence>
#include <QLabel>
#include <QMenuBar>
#include <QSplitter>
#include <QStatusBar>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWidget>

#include <spdlog/spdlog.h>

#include "PlanView.hpp"
#include "SelectTool.hpp"
#include "Viewport3D.hpp"
#include "WallTool.hpp"

namespace cadino::app {

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("Cadino");
    resize(1280, 800);

    build_central_widget();
    build_menu();
    build_toolbar();

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
    file_menu->addAction("&New");
    file_menu->addAction("&Open...");
    file_menu->addAction("&Save");
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
    redo_action_->setShortcuts({QKeySequence::Redo, QKeySequence("Ctrl+Y")});
    connect(redo_action_, &QAction::triggered, this, [this] {
        stack_.redo();
        plan_view_->update();
        viewport_3d_->update();
        update_undo_redo_actions();
    });
    update_undo_redo_actions();

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

}  // namespace cadino::app
