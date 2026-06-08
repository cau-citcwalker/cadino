#pragma once

#include <vector>

#include <QWidget>

#include "Selection.hpp"

class QDoubleSpinBox;
class QFormLayout;
class QLabel;

namespace cadino::core {
class Document;
class CommandStack;
}  // namespace cadino::core

namespace cadino::ui {

class PlanView;

class PropertiesPanel : public QWidget {
    Q_OBJECT

public:
    PropertiesPanel(cadino::core::Document& doc, cadino::core::CommandStack& stack,
                    PlanView& view, QWidget* parent = nullptr);

public slots:
    void set_selection(const Selection& sel);
    void refresh();

private:
    void build_for_wall();
    void build_for_box();
    void build_for_cylinder();
    void show_empty(const QString& message);
    void clear_form();

    QDoubleSpinBox* make_mm_field(double value);

    void commit_wall_edit();
    void commit_box_edit();
    void commit_cylinder_edit();

    cadino::core::Document& document_;
    cadino::core::CommandStack& stack_;
    PlanView& view_;

    Selection current_{};

    QFormLayout* form_{nullptr};
    QLabel* title_{nullptr};
    QLabel* empty_label_{nullptr};
    std::vector<QDoubleSpinBox*> fields_;
    bool suppress_commit_{false};
};

}  // namespace cadino::ui
