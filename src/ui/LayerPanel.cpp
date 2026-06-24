#include "LayerPanel.hpp"

#include <algorithm>
#include <vector>

#include <QColorDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QPushButton>
#include <QRadioButton>
#include <QTableWidget>
#include <QVBoxLayout>

#include "PlanView.hpp"
#include "Viewport3D.hpp"
#include "document/Document.hpp"

namespace cadino::ui {

namespace {

QColor color_to_qcolor(const cadino::core::Color& c) {
    return QColor::fromRgbF(c.r, c.g, c.b);
}

cadino::core::Color qcolor_to_color(const QColor& c) {
    return {static_cast<float>(c.redF()),
            static_cast<float>(c.greenF()),
            static_cast<float>(c.blueF())};
}

}  // namespace

LayerPanel::LayerPanel(cadino::core::Document& doc, PlanView& view, QWidget* parent)
    : QWidget(parent), document_(doc), view_(view) {
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(6, 6, 6, 6);
    outer->setSpacing(4);

    table_ = new QTableWidget(this);
    table_->setColumnCount(5);
    table_->setHorizontalHeaderLabels({"Act", "Vis", "Lock", "Color", "Name"});
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    table_->verticalHeader()->setVisible(false);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setEditTriggers(QAbstractItemView::DoubleClicked |
                            QAbstractItemView::EditKeyPressed);
    outer->addWidget(table_);

    auto* btn_row = new QHBoxLayout();
    add_btn_ = new QPushButton("Add", this);
    remove_btn_ = new QPushButton("Delete", this);
    btn_row->addWidget(add_btn_);
    btn_row->addWidget(remove_btn_);
    btn_row->addStretch();
    outer->addLayout(btn_row);

    connect(add_btn_, &QPushButton::clicked, this, &LayerPanel::on_add_layer);
    connect(remove_btn_, &QPushButton::clicked, this, &LayerPanel::on_remove_layer);
    connect(table_, &QTableWidget::cellChanged, this, &LayerPanel::on_cell_changed);

    setMinimumWidth(260);
    refresh();
}

void LayerPanel::refresh() {
    rebuild_table();
}

void LayerPanel::rebuild_table() {
    suppress_signals_ = true;

    std::vector<std::pair<cadino::core::EntityId, cadino::core::Layer>> ordered;
    ordered.reserve(document_.layers().size());
    for (const auto& [id, l] : document_.layers()) ordered.emplace_back(id, l);
    std::sort(ordered.begin(), ordered.end(),
              [](const auto& a, const auto& b) { return a.first.value < b.first.value; });

    table_->setRowCount(static_cast<int>(ordered.size()));
    for (int row = 0; row < static_cast<int>(ordered.size()); ++row) {
        const auto& [id, layer] = ordered[row];

        auto* active_btn = new QRadioButton(table_);
        active_btn->setChecked(id == document_.active_layer());
        connect(active_btn, &QRadioButton::toggled, this,
                [this, row](bool on) {
                    if (on && !suppress_signals_) on_set_active(row);
                });
        table_->setCellWidget(row, 0, active_btn);

        auto* vis_item = new QTableWidgetItem();
        vis_item->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
        vis_item->setCheckState(layer.visible ? Qt::Checked : Qt::Unchecked);
        table_->setItem(row, 1, vis_item);

        auto* lock_item = new QTableWidgetItem();
        lock_item->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
        lock_item->setCheckState(layer.locked ? Qt::Checked : Qt::Unchecked);
        table_->setItem(row, 2, lock_item);

        auto* color_btn = new QPushButton(table_);
        color_btn->setFixedHeight(20);
        const QColor col = color_to_qcolor(layer.color);
        color_btn->setStyleSheet(QString("background-color: %1; border: 1px solid #555;")
                                     .arg(col.name()));
        connect(color_btn, &QPushButton::clicked, this, [this, id, color_btn] {
            const auto* layer = document_.find_layer(id);
            if (!layer) return;
            const QColor initial = color_to_qcolor(layer->color);
            const QColor picked = QColorDialog::getColor(initial, this, "Pick layer color");
            if (!picked.isValid()) return;
            auto* mut = document_.find_layer(id);
            if (!mut) return;
            mut->color = qcolor_to_color(picked);
            color_btn->setStyleSheet(QString("background-color: %1; border: 1px solid #555;")
                                         .arg(picked.name()));
            emit_view_refresh();
            emit layers_changed();
        });
        table_->setCellWidget(row, 3, color_btn);

        auto* name_item = new QTableWidgetItem(QString::fromStdString(layer.name));
        name_item->setData(Qt::UserRole, qint64(id.value));
        name_item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable);
        table_->setItem(row, 4, name_item);
    }

    suppress_signals_ = false;
}

void LayerPanel::on_cell_changed(int row, int col) {
    if (suppress_signals_) return;
    auto* name_item = table_->item(row, 4);
    if (!name_item) return;
    const auto id = cadino::core::EntityId{
        static_cast<std::uint64_t>(name_item->data(Qt::UserRole).toULongLong())};
    auto* layer = document_.find_layer(id);
    if (!layer) return;

    if (col == 1) {
        layer->visible = table_->item(row, 1)->checkState() == Qt::Checked;
        emit_view_refresh();
    } else if (col == 2) {
        layer->locked = table_->item(row, 2)->checkState() == Qt::Checked;
        emit_view_refresh();
    } else if (col == 4) {
        layer->name = name_item->text().toStdString();
    }
    emit layers_changed();
}

void LayerPanel::on_add_layer() {
    bool ok = false;
    const QString name = QInputDialog::getText(this, "New layer", "Layer name:",
                                               QLineEdit::Normal, "New Layer", &ok);
    if (!ok || name.trimmed().isEmpty()) return;
    cadino::core::Layer layer;
    layer.name = name.toStdString();
    document_.add_layer(std::move(layer));
    refresh();
    emit layers_changed();
}

void LayerPanel::on_remove_layer() {
    const int row = table_->currentRow();
    if (row < 0) return;
    auto* name_item = table_->item(row, 4);
    if (!name_item) return;
    const auto id = cadino::core::EntityId{
        static_cast<std::uint64_t>(name_item->data(Qt::UserRole).toULongLong())};
    if (!document_.remove_layer(id)) return;
    refresh();
    emit_view_refresh();
    emit layers_changed();
}

void LayerPanel::on_set_active(int row) {
    auto* name_item = table_->item(row, 4);
    if (!name_item) return;
    const auto id = cadino::core::EntityId{
        static_cast<std::uint64_t>(name_item->data(Qt::UserRole).toULongLong())};
    document_.set_active_layer(id);
    emit active_layer_changed();
}

void LayerPanel::emit_view_refresh() {
    view_.update();
    view_.notify_document_modified();
    if (viewport_3d_) viewport_3d_->refresh();
}

}  // namespace cadino::ui
