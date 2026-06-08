#pragma once

#include <QMainWindow>

#include "command/CommandStack.hpp"
#include "document/Document.hpp"

class QAction;
class QSplitter;
class QWidget;

namespace cadino::ui {
class PlanView;
}

namespace cadino::app {

enum class ViewMode {
    PlanOnly,
    ViewportOnly,
    Split,
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
    void set_view_mode(ViewMode mode);
    void update_undo_redo_actions();

    cadino::core::Document document_;
    cadino::core::CommandStack stack_{document_};

    QSplitter* splitter_{nullptr};
    cadino::ui::PlanView* plan_view_{nullptr};
    QWidget* viewport_3d_{nullptr};

    QAction* undo_action_{nullptr};
    QAction* redo_action_{nullptr};
    QAction* select_action_{nullptr};
    QAction* wall_action_{nullptr};
    QAction* mode_plan_action_{nullptr};
    QAction* mode_viewport_action_{nullptr};
    QAction* mode_split_action_{nullptr};

    ViewMode view_mode_{ViewMode::Split};
};

}  // namespace cadino::app
