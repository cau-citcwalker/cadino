#include "PropertiesPanel.hpp"

#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QVBoxLayout>

#include "PlanView.hpp"
#include "command/BoxCommands.hpp"
#include "command/CommandStack.hpp"
#include "command/WallCommands.hpp"
#include "document/Document.hpp"

namespace cadino::ui {

PropertiesPanel::PropertiesPanel(cadino::core::Document& doc,
                                 cadino::core::CommandStack& stack,
                                 PlanView& view, QWidget* parent)
    : QWidget(parent), document_(doc), stack_(stack), view_(view) {
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(8, 8, 8, 8);
    outer->setSpacing(6);

    title_ = new QLabel("Properties", this);
    QFont f = title_->font();
    f.setBold(true);
    title_->setFont(f);
    outer->addWidget(title_);

    auto* form_container = new QWidget(this);
    form_ = new QFormLayout(form_container);
    form_->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(form_container);

    empty_label_ = new QLabel("No selection", this);
    empty_label_->setStyleSheet("color: #888;");
    outer->addWidget(empty_label_);

    outer->addStretch();
    setMinimumWidth(260);

    show_empty("No selection");
}

void PropertiesPanel::set_selection(const Selection& sel) {
    current_ = sel;
    refresh();
}

void PropertiesPanel::refresh() {
    if (!current_.valid()) {
        show_empty("No selection");
        return;
    }
    if (current_.kind == SelectKind::Wall) {
        if (!document_.find_wall(current_.id)) {
            show_empty("Wall no longer exists");
            return;
        }
        build_for_wall();
    } else if (current_.kind == SelectKind::Box) {
        if (!document_.find_box(current_.id)) {
            show_empty("Box no longer exists");
            return;
        }
        build_for_box();
    }
}

void PropertiesPanel::clear_form() {
    fields_.clear();
    while (form_->rowCount() > 0) {
        form_->removeRow(0);
    }
}

void PropertiesPanel::show_empty(const QString& message) {
    clear_form();
    title_->setText("Properties");
    empty_label_->setText(message);
    empty_label_->setVisible(true);
}

QDoubleSpinBox* PropertiesPanel::make_mm_field(double value) {
    auto* box = new QDoubleSpinBox(this);
    box->setRange(-1'000'000.0, 1'000'000.0);
    box->setDecimals(1);
    box->setSingleStep(10.0);
    box->setSuffix(" mm");
    box->setValue(value);
    box->setKeyboardTracking(false);
    return box;
}

void PropertiesPanel::build_for_wall() {
    const auto* w = document_.find_wall(current_.id);
    if (!w) {
        show_empty("Wall not found");
        return;
    }

    suppress_commit_ = true;
    clear_form();
    empty_label_->setVisible(false);
    title_->setText("Wall");

    auto* start_x = make_mm_field(w->start.x());
    auto* start_y = make_mm_field(w->start.y());
    auto* end_x = make_mm_field(w->end.x());
    auto* end_y = make_mm_field(w->end.y());
    auto* height = make_mm_field(w->height);
    height->setMinimum(1.0);
    auto* thickness = make_mm_field(w->thickness);
    thickness->setMinimum(1.0);

    form_->addRow("Start X", start_x);
    form_->addRow("Start Y", start_y);
    form_->addRow("End X", end_x);
    form_->addRow("End Y", end_y);
    form_->addRow("Height", height);
    form_->addRow("Thickness", thickness);

    fields_ = {start_x, start_y, end_x, end_y, height, thickness};

    for (auto* field : fields_) {
        connect(field, &QDoubleSpinBox::editingFinished, this,
                &PropertiesPanel::commit_wall_edit);
    }
    suppress_commit_ = false;
}

void PropertiesPanel::build_for_box() {
    const auto* b = document_.find_box(current_.id);
    if (!b) {
        show_empty("Box not found");
        return;
    }

    suppress_commit_ = true;
    clear_form();
    empty_label_->setVisible(false);
    title_->setText("Box");

    auto* pos_x = make_mm_field(b->position.x());
    auto* pos_y = make_mm_field(b->position.y());
    auto* size_x = make_mm_field(b->size_xy.x());
    size_x->setMinimum(1.0);
    auto* size_y = make_mm_field(b->size_xy.y());
    size_y->setMinimum(1.0);
    auto* height = make_mm_field(b->height);
    height->setMinimum(1.0);
    auto* base_z = make_mm_field(b->base_z);
    auto* rotation = new QDoubleSpinBox(this);
    rotation->setRange(-360.0, 360.0);
    rotation->setDecimals(1);
    rotation->setSingleStep(5.0);
    rotation->setSuffix(" deg");
    rotation->setValue(b->rotation_z * 180.0 / 3.14159265358979323846);
    rotation->setKeyboardTracking(false);

    form_->addRow("Position X", pos_x);
    form_->addRow("Position Y", pos_y);
    form_->addRow("Size X (width)", size_x);
    form_->addRow("Size Y (depth)", size_y);
    form_->addRow("Height", height);
    form_->addRow("Base Z", base_z);
    form_->addRow("Rotation Z", rotation);

    fields_ = {pos_x, pos_y, size_x, size_y, height, base_z, rotation};

    for (auto* field : fields_) {
        connect(field, &QDoubleSpinBox::editingFinished, this,
                &PropertiesPanel::commit_box_edit);
    }
    suppress_commit_ = false;
}

void PropertiesPanel::commit_wall_edit() {
    if (suppress_commit_ || !current_.valid() || fields_.size() != 6) return;
    const auto* w = document_.find_wall(current_.id);
    if (!w) return;

    cadino::core::Wall after = *w;
    after.start.x() = fields_[0]->value();
    after.start.y() = fields_[1]->value();
    after.end.x() = fields_[2]->value();
    after.end.y() = fields_[3]->value();
    after.height = fields_[4]->value();
    after.thickness = fields_[5]->value();

    if (after.start == w->start && after.end == w->end &&
        after.height == w->height && after.thickness == w->thickness) {
        return;
    }

    stack_.execute(std::make_unique<cadino::core::ModifyWallCommand>(current_.id, after));
    view_.notify_document_modified();
}

void PropertiesPanel::commit_box_edit() {
    if (suppress_commit_ || !current_.valid() || fields_.size() != 7) return;
    const auto* b = document_.find_box(current_.id);
    if (!b) return;

    cadino::core::Box after = *b;
    after.position.x() = fields_[0]->value();
    after.position.y() = fields_[1]->value();
    after.size_xy.x() = fields_[2]->value();
    after.size_xy.y() = fields_[3]->value();
    after.height = fields_[4]->value();
    after.base_z = fields_[5]->value();
    after.rotation_z = fields_[6]->value() * 3.14159265358979323846 / 180.0;

    if (after.position == b->position && after.size_xy == b->size_xy &&
        after.height == b->height && after.base_z == b->base_z &&
        after.rotation_z == b->rotation_z) {
        return;
    }

    stack_.execute(std::make_unique<cadino::core::ModifyBoxCommand>(current_.id, after));
    view_.notify_document_modified();
}

}  // namespace cadino::ui
