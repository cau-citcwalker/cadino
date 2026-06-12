#include "PropertiesPanel.hpp"

#include <QColorDialog>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include "PlanView.hpp"
#include "command/BlockCommands.hpp"
#include "command/BlockInstanceCommands.hpp"
#include "command/BoxCommands.hpp"
#include "command/CommandStack.hpp"
#include "command/CylinderCommands.hpp"
#include "command/NurbsCurveCommands.hpp"
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
    } else if (current_.kind == SelectKind::Cylinder) {
        if (!document_.find_cylinder(current_.id)) {
            show_empty("Cylinder no longer exists");
            return;
        }
        build_for_cylinder();
    } else if (current_.kind == SelectKind::NurbsCurve) {
        if (!document_.find_curve(current_.id)) {
            show_empty("Curve no longer exists");
            return;
        }
        build_for_curve();
    } else if (current_.kind == SelectKind::Block) {
        if (!document_.find_block(current_.id)) {
            show_empty("Block no longer exists");
            return;
        }
        build_for_block();
    } else if (current_.kind == SelectKind::BlockInstance) {
        if (!document_.find_block_instance(current_.id)) {
            show_empty("Block instance no longer exists");
            return;
        }
        build_for_block_instance();
    }
}

void PropertiesPanel::clear_form() {
    fields_.clear();
    roughness_field_ = nullptr;
    metallic_field_ = nullptr;
    pattern_combo_ = nullptr;
    color_button_ = nullptr;
    texture_button_ = nullptr;
    current_texture_path_.clear();
    while (form_->rowCount() > 0) {
        form_->removeRow(0);
    }
}

namespace {

QString texture_label_for(const QString& path) {
    if (path.isEmpty()) return QStringLiteral("Choose…");
    return QFileInfo(path).fileName();
}

QComboBox* make_pattern_combo(QWidget* parent, int current) {
    auto* combo = new QComboBox(parent);
    combo->addItem("Solid");
    combo->addItem("Checker (500 mm)");
    combo->addItem("Stripes (100 mm)");
    combo->addItem("Wood grain");
    combo->setCurrentIndex(std::clamp(current, 0, 3));
    return combo;
}

}  // namespace

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

QDoubleSpinBox* PropertiesPanel::make_unit_field(double value) {
    auto* box = new QDoubleSpinBox(this);
    box->setRange(0.0, 1.0);
    box->setDecimals(2);
    box->setSingleStep(0.05);
    box->setValue(value);
    box->setKeyboardTracking(false);
    return box;
}

QPushButton* PropertiesPanel::make_color_button(float r, float g, float b) {
    auto* btn = new QPushButton(this);
    btn->setFixedHeight(28);
    current_color_ = {r, g, b};
    btn->setStyleSheet(QString("background-color: rgb(%1, %2, %3); border: 1px solid #555;")
                           .arg(int(r * 255)).arg(int(g * 255)).arg(int(b * 255)));
    btn->setText("");
    connect(btn, &QPushButton::clicked, this, [this, btn] {
        const QColor initial = QColor::fromRgbF(current_color_.r, current_color_.g,
                                                current_color_.b);
        const QColor picked = QColorDialog::getColor(initial, this, "Pick entity color");
        if (!picked.isValid()) return;
        current_color_ = {static_cast<float>(picked.redF()),
                          static_cast<float>(picked.greenF()),
                          static_cast<float>(picked.blueF())};
        btn->setStyleSheet(QString("background-color: %1; border: 1px solid #555;")
                               .arg(picked.name()));
        if (current_.kind == SelectKind::Wall) commit_wall_edit();
        else if (current_.kind == SelectKind::Box) commit_box_edit();
        else if (current_.kind == SelectKind::Cylinder) commit_cylinder_edit();
    });
    return btn;
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

    color_button_ = make_color_button(w->color.r, w->color.g, w->color.b);
    roughness_field_ = make_unit_field(w->roughness);
    metallic_field_ = make_unit_field(w->metallic);
    pattern_combo_ = make_pattern_combo(this, w->pattern);
    current_texture_path_ = QString::fromStdString(w->texture_path);
    texture_button_ = new QPushButton(texture_label_for(current_texture_path_), this);
    connect(texture_button_, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getOpenFileName(
            this, "Choose albedo texture", current_texture_path_,
            "Images (*.png *.jpg *.jpeg *.bmp *.tga);;All files (*)");
        if (path.isEmpty() && !current_texture_path_.isEmpty()) {
            // Allow clearing by clicking and cancelling intentionally — use
            // a separate action below instead.
            return;
        }
        current_texture_path_ = path;
        if (texture_button_) texture_button_->setText(texture_label_for(path));
        commit_wall_edit();
    });

    form_->addRow("Start X", start_x);
    form_->addRow("Start Y", start_y);
    form_->addRow("End X", end_x);
    form_->addRow("End Y", end_y);
    form_->addRow("Height", height);
    form_->addRow("Thickness", thickness);
    form_->addRow("Color", color_button_);
    form_->addRow("Roughness", roughness_field_);
    form_->addRow("Metallic", metallic_field_);
    form_->addRow("Pattern", pattern_combo_);
    form_->addRow("Texture", texture_button_);

    fields_ = {start_x, start_y, end_x, end_y, height, thickness};

    for (auto* field : fields_) {
        connect(field, &QDoubleSpinBox::editingFinished, this,
                &PropertiesPanel::commit_wall_edit);
    }
    connect(roughness_field_, &QDoubleSpinBox::editingFinished, this,
            &PropertiesPanel::commit_wall_edit);
    connect(metallic_field_, &QDoubleSpinBox::editingFinished, this,
            &PropertiesPanel::commit_wall_edit);
    connect(pattern_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int) { commit_wall_edit(); });
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

    color_button_ = make_color_button(b->color.r, b->color.g, b->color.b);
    roughness_field_ = make_unit_field(b->roughness);
    metallic_field_ = make_unit_field(b->metallic);
    pattern_combo_ = make_pattern_combo(this, b->pattern);
    current_texture_path_ = QString::fromStdString(b->texture_path);
    texture_button_ = new QPushButton(texture_label_for(current_texture_path_), this);
    connect(texture_button_, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getOpenFileName(
            this, "Choose albedo texture", current_texture_path_,
            "Images (*.png *.jpg *.jpeg *.bmp *.tga);;All files (*)");
        if (path.isEmpty()) return;
        current_texture_path_ = path;
        if (texture_button_) texture_button_->setText(texture_label_for(path));
        commit_box_edit();
    });

    form_->addRow("Position X", pos_x);
    form_->addRow("Position Y", pos_y);
    form_->addRow("Size X (width)", size_x);
    form_->addRow("Size Y (depth)", size_y);
    form_->addRow("Height", height);
    form_->addRow("Base Z", base_z);
    form_->addRow("Rotation Z", rotation);
    form_->addRow("Color", color_button_);
    form_->addRow("Roughness", roughness_field_);
    form_->addRow("Metallic", metallic_field_);
    form_->addRow("Pattern", pattern_combo_);
    form_->addRow("Texture", texture_button_);

    fields_ = {pos_x, pos_y, size_x, size_y, height, base_z, rotation};

    for (auto* field : fields_) {
        connect(field, &QDoubleSpinBox::editingFinished, this,
                &PropertiesPanel::commit_box_edit);
    }
    connect(roughness_field_, &QDoubleSpinBox::editingFinished, this,
            &PropertiesPanel::commit_box_edit);
    connect(metallic_field_, &QDoubleSpinBox::editingFinished, this,
            &PropertiesPanel::commit_box_edit);
    connect(pattern_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int) { commit_box_edit(); });
    suppress_commit_ = false;
}

void PropertiesPanel::build_for_cylinder() {
    const auto* c = document_.find_cylinder(current_.id);
    if (!c) {
        show_empty("Cylinder not found");
        return;
    }

    suppress_commit_ = true;
    clear_form();
    empty_label_->setVisible(false);
    title_->setText("Cylinder");

    auto* pos_x = make_mm_field(c->position.x());
    auto* pos_y = make_mm_field(c->position.y());
    auto* radius = make_mm_field(c->radius);
    radius->setMinimum(1.0);
    auto* height = make_mm_field(c->height);
    height->setMinimum(1.0);
    auto* base_z = make_mm_field(c->base_z);

    color_button_ = make_color_button(c->color.r, c->color.g, c->color.b);
    roughness_field_ = make_unit_field(c->roughness);
    metallic_field_ = make_unit_field(c->metallic);
    pattern_combo_ = make_pattern_combo(this, c->pattern);
    current_texture_path_ = QString::fromStdString(c->texture_path);
    texture_button_ = new QPushButton(texture_label_for(current_texture_path_), this);
    connect(texture_button_, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getOpenFileName(
            this, "Choose albedo texture", current_texture_path_,
            "Images (*.png *.jpg *.jpeg *.bmp *.tga);;All files (*)");
        if (path.isEmpty()) return;
        current_texture_path_ = path;
        if (texture_button_) texture_button_->setText(texture_label_for(path));
        commit_cylinder_edit();
    });

    form_->addRow("Position X", pos_x);
    form_->addRow("Position Y", pos_y);
    form_->addRow("Radius", radius);
    form_->addRow("Height", height);
    form_->addRow("Base Z", base_z);
    form_->addRow("Color", color_button_);
    form_->addRow("Roughness", roughness_field_);
    form_->addRow("Metallic", metallic_field_);
    form_->addRow("Pattern", pattern_combo_);
    form_->addRow("Texture", texture_button_);

    fields_ = {pos_x, pos_y, radius, height, base_z};

    for (auto* field : fields_) {
        connect(field, &QDoubleSpinBox::editingFinished, this,
                &PropertiesPanel::commit_cylinder_edit);
    }
    connect(roughness_field_, &QDoubleSpinBox::editingFinished, this,
            &PropertiesPanel::commit_cylinder_edit);
    connect(metallic_field_, &QDoubleSpinBox::editingFinished, this,
            &PropertiesPanel::commit_cylinder_edit);
    connect(pattern_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int) { commit_cylinder_edit(); });
    suppress_commit_ = false;
}

void PropertiesPanel::commit_cylinder_edit() {
    if (suppress_commit_ || !current_.valid() || fields_.size() != 5) return;
    const auto* c = document_.find_cylinder(current_.id);
    if (!c) return;

    cadino::core::Cylinder after = *c;
    after.position.x() = fields_[0]->value();
    after.position.y() = fields_[1]->value();
    after.radius = fields_[2]->value();
    after.height = fields_[3]->value();
    after.base_z = fields_[4]->value();
    after.color = current_color_;
    if (roughness_field_) after.roughness = static_cast<float>(roughness_field_->value());
    if (metallic_field_) after.metallic = static_cast<float>(metallic_field_->value());
    if (pattern_combo_) after.pattern = pattern_combo_->currentIndex();
    after.texture_path = current_texture_path_.toStdString();

    if (after.position == c->position && after.radius == c->radius &&
        after.height == c->height && after.base_z == c->base_z &&
        after.color == c->color && after.roughness == c->roughness &&
        after.metallic == c->metallic && after.pattern == c->pattern &&
        after.texture_path == c->texture_path) {
        return;
    }

    stack_.execute(std::make_unique<cadino::core::ModifyCylinderCommand>(current_.id, after));
    view_.notify_document_modified();
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
    after.color = current_color_;
    if (roughness_field_) after.roughness = static_cast<float>(roughness_field_->value());
    if (metallic_field_) after.metallic = static_cast<float>(metallic_field_->value());
    if (pattern_combo_) after.pattern = pattern_combo_->currentIndex();
    after.texture_path = current_texture_path_.toStdString();

    if (after.start == w->start && after.end == w->end &&
        after.height == w->height && after.thickness == w->thickness &&
        after.color == w->color && after.roughness == w->roughness &&
        after.metallic == w->metallic && after.pattern == w->pattern &&
        after.texture_path == w->texture_path) {
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
    after.color = current_color_;
    if (roughness_field_) after.roughness = static_cast<float>(roughness_field_->value());
    if (metallic_field_) after.metallic = static_cast<float>(metallic_field_->value());
    if (pattern_combo_) after.pattern = pattern_combo_->currentIndex();
    after.texture_path = current_texture_path_.toStdString();

    if (after.position == b->position && after.size_xy == b->size_xy &&
        after.height == b->height && after.base_z == b->base_z &&
        after.rotation_z == b->rotation_z && after.color == b->color &&
        after.roughness == b->roughness && after.metallic == b->metallic &&
        after.pattern == b->pattern && after.texture_path == b->texture_path) {
        return;
    }

    stack_.execute(std::make_unique<cadino::core::ModifyBoxCommand>(current_.id, after));
    view_.notify_document_modified();
}

void PropertiesPanel::build_for_curve() {
    const auto* c = document_.find_curve(current_.id);
    if (!c) {
        show_empty("Curve not found");
        return;
    }
    suppress_commit_ = true;
    clear_form();
    empty_label_->setVisible(false);
    title_->setText("NURBS Curve");

    auto* degree = new QDoubleSpinBox(this);
    degree->setRange(1, 5);
    degree->setDecimals(0);
    degree->setSingleStep(1);
    degree->setValue(c->degree);
    degree->setKeyboardTracking(false);

    auto* line_width = new QDoubleSpinBox(this);
    line_width->setRange(0.5, 10.0);
    line_width->setDecimals(1);
    line_width->setSingleStep(0.5);
    line_width->setValue(c->line_width);
    line_width->setKeyboardTracking(false);

    color_button_ = make_color_button(c->color.r, c->color.g, c->color.b);

    form_->addRow("Degree", degree);
    form_->addRow("Line width", line_width);
    form_->addRow("Color", color_button_);
    form_->addRow("Control points",
                  new QLabel(QString::number(c->control_points.size()), this));

    fields_ = {degree, line_width};
    for (auto* field : fields_) {
        connect(field, &QDoubleSpinBox::editingFinished, this,
                &PropertiesPanel::commit_curve_edit);
    }
    suppress_commit_ = false;
}

void PropertiesPanel::commit_curve_edit() {
    if (suppress_commit_ || !current_.valid() || fields_.size() != 2) return;
    const auto* c = document_.find_curve(current_.id);
    if (!c) return;

    cadino::core::NurbsCurve after = *c;
    after.degree = static_cast<int>(fields_[0]->value());
    after.line_width = static_cast<float>(fields_[1]->value());
    after.color = current_color_;

    if (after.degree == c->degree && after.line_width == c->line_width &&
        after.color == c->color) {
        return;
    }
    stack_.execute(std::make_unique<cadino::core::ModifyNurbsCurveCommand>(current_.id, after));
    view_.notify_document_modified();
}

void PropertiesPanel::build_for_block() {
    const auto* bl = document_.find_block(current_.id);
    if (!bl) {
        show_empty("Block not found");
        return;
    }
    suppress_commit_ = true;
    clear_form();
    empty_label_->setVisible(false);
    title_->setText("Block");

    auto* pos_x = make_mm_field(bl->position.x());
    auto* pos_y = make_mm_field(bl->position.y());
    auto* base_z = make_mm_field(bl->base_z);
    auto* rot = new QDoubleSpinBox(this);
    rot->setRange(-360.0, 360.0);
    rot->setDecimals(1);
    rot->setSingleStep(5.0);
    rot->setSuffix(" deg");
    rot->setValue(bl->rotation_z * 180.0 / 3.14159265358979323846);
    rot->setKeyboardTracking(false);

    form_->addRow("Name",
                  new QLabel(QString::fromStdString(bl->name), this));
    form_->addRow("Position X", pos_x);
    form_->addRow("Position Y", pos_y);
    form_->addRow("Base Z", base_z);
    form_->addRow("Rotation Z", rot);
    form_->addRow("Boxes",
                  new QLabel(QString::number(bl->boxes.size()), this));
    form_->addRow("Cylinders",
                  new QLabel(QString::number(bl->cylinders.size()), this));

    fields_ = {pos_x, pos_y, base_z, rot};
    for (auto* field : fields_) {
        connect(field, &QDoubleSpinBox::editingFinished, this,
                &PropertiesPanel::commit_block_edit);
    }
    suppress_commit_ = false;
}

void PropertiesPanel::commit_block_edit() {
    if (suppress_commit_ || !current_.valid() || fields_.size() != 4) return;
    const auto* bl = document_.find_block(current_.id);
    if (!bl) return;

    cadino::core::Block after = *bl;
    after.position.x() = fields_[0]->value();
    after.position.y() = fields_[1]->value();
    after.base_z = fields_[2]->value();
    after.rotation_z = fields_[3]->value() * 3.14159265358979323846 / 180.0;

    if (after.position == bl->position && after.base_z == bl->base_z &&
        after.rotation_z == bl->rotation_z) {
        return;
    }
    stack_.execute(std::make_unique<cadino::core::ModifyBlockCommand>(current_.id, after));
    view_.notify_document_modified();
}

void PropertiesPanel::build_for_block_instance() {
    const auto* inst = document_.find_block_instance(current_.id);
    if (!inst) {
        show_empty("Block instance not found");
        return;
    }
    suppress_commit_ = true;
    clear_form();
    empty_label_->setVisible(false);
    title_->setText("Block Instance");

    QString def_name = "(missing)";
    if (const auto* def = document_.find_block_def(inst->definition_id)) {
        def_name = QString::fromStdString(def->name);
    }

    auto* pos_x = make_mm_field(inst->position.x());
    auto* pos_y = make_mm_field(inst->position.y());
    auto* base_z = make_mm_field(inst->base_z);
    auto* rot = new QDoubleSpinBox(this);
    rot->setRange(-360.0, 360.0);
    rot->setDecimals(1);
    rot->setSingleStep(5.0);
    rot->setSuffix(" deg");
    rot->setValue(inst->rotation_z * 180.0 / 3.14159265358979323846);
    rot->setKeyboardTracking(false);

    form_->addRow("Definition", new QLabel(def_name, this));
    form_->addRow("Position X", pos_x);
    form_->addRow("Position Y", pos_y);
    form_->addRow("Base Z", base_z);
    form_->addRow("Rotation Z", rot);

    fields_ = {pos_x, pos_y, base_z, rot};
    for (auto* field : fields_) {
        connect(field, &QDoubleSpinBox::editingFinished, this,
                &PropertiesPanel::commit_block_instance_edit);
    }
    suppress_commit_ = false;
}

void PropertiesPanel::commit_block_instance_edit() {
    if (suppress_commit_ || !current_.valid() || fields_.size() != 4) return;
    const auto* inst = document_.find_block_instance(current_.id);
    if (!inst) return;

    cadino::core::BlockInstance after = *inst;
    after.position.x() = fields_[0]->value();
    after.position.y() = fields_[1]->value();
    after.base_z = fields_[2]->value();
    after.rotation_z = fields_[3]->value() * 3.14159265358979323846 / 180.0;

    if (after.position == inst->position && after.base_z == inst->base_z &&
        after.rotation_z == inst->rotation_z) {
        return;
    }
    stack_.execute(std::make_unique<cadino::core::ModifyBlockInstanceCommand>(current_.id, after));
    view_.notify_document_modified();
}

}  // namespace cadino::ui
