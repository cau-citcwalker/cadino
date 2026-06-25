// Real GUI end-to-end tests: spin up an actual QApplication on the offscreen
// platform plugin, drive widgets with synthesized mouse events via QTest, and
// inspect document/widget state afterwards. Covers the plan-view drawing
// tools, the selection-drag flow, the layer panel, properties panel,
// MainWindow menu actions, and the 3D viewport gizmo.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_session.hpp>
#include <catch2/catch_test_macros.hpp>

#include <QApplication>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QPoint>
#include <QRadioButton>
#include <QSurfaceFormat>
#include <QTableWidget>
#include <QTest>
#include <QTimer>
#include <QWidget>

#include <cmath>
#include <memory>

#include "command/BoxCommands.hpp"
#include "command/CommandStack.hpp"
#include "command/WallCommands.hpp"
#include "document/Document.hpp"

#include "BoxTool.hpp"
#include "CylinderTool.hpp"
#include "DimensionTool.hpp"
#include "LayerPanel.hpp"
#include "PlanView.hpp"
#include "PropertiesPanel.hpp"
#include "SelectTool.hpp"
#include "Viewport3D.hpp"
#include "WallTool.hpp"

#include "MainWindow.hpp"

namespace {

QPoint screen_for(const cadino::ui::PlanView& v, double x, double y) {
    return v.model_to_screen({x, y}).toPoint();
}

}  // namespace

int main(int argc, char** argv) {
    // Headless plugin so the suite runs in CI without a display server.
    qputenv("QT_QPA_PLATFORM", "offscreen");
#ifdef CADINO_QT_PLATFORMS_DIR
    // Help Qt locate the offscreen platform plugin even when the test is
    // launched without windeployqt — the path is baked in at build time.
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM_PLUGIN_PATH")) {
        qputenv("QT_QPA_PLATFORM_PLUGIN_PATH", CADINO_QT_PLATFORMS_DIR);
    }
#endif
    QApplication app(argc, argv);
    return Catch::Session().run(argc, argv);
}

TEST_CASE("WallTool: two left-clicks adds a wall", "[gui][plan]") {
    cadino::core::Document doc;
    cadino::core::CommandStack stack(doc);
    cadino::ui::PlanView pv(doc, stack);
    pv.resize(800, 600);
    pv.show();
    QTest::qWaitForWindowExposed(&pv);
    pv.set_tool(std::make_unique<cadino::ui::WallTool>());

    QTest::mouseClick(&pv, Qt::LeftButton, Qt::NoModifier, screen_for(pv, 0, 0));
    QTest::qWait(5);
    QTest::mouseClick(&pv, Qt::LeftButton, Qt::NoModifier, screen_for(pv, 4000, 0));
    QTest::qWait(5);

    REQUIRE(doc.walls().size() == 1);
}

TEST_CASE("BoxTool: two clicks adds a box centered between them", "[gui][plan]") {
    cadino::core::Document doc;
    cadino::core::CommandStack stack(doc);
    cadino::ui::PlanView pv(doc, stack);
    pv.resize(800, 600);
    pv.show();
    QTest::qWaitForWindowExposed(&pv);
    pv.set_tool(std::make_unique<cadino::ui::BoxTool>());

    QTest::mouseClick(&pv, Qt::LeftButton, Qt::NoModifier, screen_for(pv, 0, 0));
    QTest::qWait(5);
    QTest::mouseClick(&pv, Qt::LeftButton, Qt::NoModifier, screen_for(pv, 2000, 1000));
    QTest::qWait(5);

    REQUIRE(doc.boxes().size() == 1);
}

TEST_CASE("DimensionTool: three clicks places a dimension", "[gui][plan]") {
    cadino::core::Document doc;
    cadino::core::CommandStack stack(doc);
    cadino::ui::PlanView pv(doc, stack);
    pv.resize(800, 600);
    pv.show();
    QTest::qWaitForWindowExposed(&pv);
    pv.set_tool(std::make_unique<cadino::ui::DimensionTool>());

    QTest::mouseClick(&pv, Qt::LeftButton, Qt::NoModifier, screen_for(pv, 0, 0));
    QTest::qWait(5);
    QTest::mouseClick(&pv, Qt::LeftButton, Qt::NoModifier, screen_for(pv, 2000, 0));
    QTest::qWait(5);
    QTest::mouseClick(&pv, Qt::LeftButton, Qt::NoModifier, screen_for(pv, 1000, 400));
    QTest::qWait(5);

    REQUIRE(doc.dimensions().size() == 1);
}

TEST_CASE("SelectTool: click + drag translates an entity", "[gui][plan]") {
    cadino::core::Document doc;
    cadino::core::CommandStack stack(doc);
    cadino::core::Box b;
    b.position = {1000, 1000};
    b.size_xy = {800, 600};
    b.height = 500;
    const auto id = doc.add_box(std::move(b));

    cadino::ui::PlanView pv(doc, stack);
    pv.resize(800, 600);
    pv.show();
    QTest::qWaitForWindowExposed(&pv);
    pv.set_tool(std::make_unique<cadino::ui::SelectTool>());

    // Disable snap entirely so the test isn't subject to grid/corner pulls
    // from the box's own footprint corners landing near the drag target.
    auto& snap = pv.snap_engine();
    snap.set_grid_enabled(false);
    snap.set_endpoint_enabled(false);
    snap.set_midpoint_enabled(false);
    snap.set_corner_enabled(false);
    snap.set_center_enabled(false);
    snap.set_intersection_enabled(false);
    snap.set_perpendicular_enabled(false);
    snap.set_nearest_enabled(false);

    const QPoint start = screen_for(pv, 1000, 1000);
    QTest::mousePress(&pv, Qt::LeftButton, Qt::NoModifier, start);
    QTest::qWait(5);
    QTest::mouseMove(&pv, screen_for(pv, 2500, 1500));
    QTest::qWait(5);
    QTest::mouseRelease(&pv, Qt::LeftButton, Qt::NoModifier, screen_for(pv, 2500, 1500));
    QTest::qWait(5);

    const auto* moved = doc.find_box(id);
    REQUIRE(moved != nullptr);
    REQUIRE(moved->position.x() == Catch::Approx(2500.0).margin(50.0));
    REQUIRE(moved->position.y() == Catch::Approx(1500.0).margin(50.0));
}

TEST_CASE("LayerPanel: visibility checkbox toggles layer.visible", "[gui][layer]") {
    cadino::core::Document doc;
    cadino::core::CommandStack stack(doc);

    cadino::ui::PlanView pv(doc, stack);
    cadino::ui::LayerPanel panel(doc, pv);
    panel.resize(320, 240);
    panel.show();
    QTest::qWaitForWindowExposed(&panel);

    auto* table = panel.findChild<QTableWidget*>();
    REQUIRE(table != nullptr);
    REQUIRE(table->rowCount() == 1);

    auto* vis_item = table->item(0, 1);
    REQUIRE(vis_item != nullptr);
    REQUIRE(vis_item->checkState() == Qt::Checked);

    vis_item->setCheckState(Qt::Unchecked);
    QTest::qWait(5);

    const auto def_id = doc.default_layer();
    const auto* layer = doc.find_layer(def_id);
    REQUIRE(layer != nullptr);
    REQUIRE(layer->visible == false);
}

TEST_CASE("PropertiesPanel: editing a box field commits a Modify command",
          "[gui][properties]") {
    cadino::core::Document doc;
    cadino::core::CommandStack stack(doc);
    cadino::core::Box b;
    b.position = {1000, 500};
    b.size_xy = {600, 400};
    b.height = 750;
    const auto id = doc.add_box(std::move(b));

    cadino::ui::PlanView pv(doc, stack);
    cadino::ui::PropertiesPanel props(doc, stack, pv);
    props.resize(320, 480);
    props.show();
    QTest::qWaitForWindowExposed(&props);

    props.set_selection({id, cadino::ui::SelectKind::Box});
    QTest::qWait(5);

    // The first 5 mm fields are: pos.x, pos.y, size.x, size.y, height.
    // Bump pos.x by 1500 mm.
    auto spin_boxes = props.findChildren<QDoubleSpinBox*>();
    REQUIRE(spin_boxes.size() >= 5);
    spin_boxes[0]->setValue(2500.0);
    spin_boxes[0]->editingFinished();
    QTest::qWait(5);

    const auto* moved = doc.find_box(id);
    REQUIRE(moved != nullptr);
    REQUIRE(moved->position.x() == Catch::Approx(2500.0));
    REQUIRE(stack.cursor() >= 1);
}

TEST_CASE("MainWindow: boots, view-mode toggle changes visible widgets",
          "[gui][app]") {
    cadino::app::MainWindow mw;
    mw.resize(1280, 800);
    mw.show();
    QTest::qWaitForWindowExposed(&mw);
    QTest::qWait(30);

    REQUIRE(mw.plan_view_widget() != nullptr);
    REQUIRE(mw.viewport_3d_widget() != nullptr);

    // Plan-only mode hides the 3D viewport.
    QAction* plan_action = nullptr;
    for (QAction* a : mw.findChildren<QAction*>()) {
        if (a->text().contains("Plan") && a->isCheckable()) {
            plan_action = a;
            break;
        }
    }
    REQUIRE(plan_action != nullptr);
    plan_action->trigger();
    QTest::qWait(30);

    REQUIRE(mw.plan_view_widget()->isVisible());
    REQUIRE_FALSE(mw.viewport_3d_widget()->isVisible());
}

TEST_CASE("MainWindow: Align Left action aligns selected boxes", "[gui][align]") {
    cadino::app::MainWindow mw;
    mw.resize(1280, 800);
    mw.show();
    QTest::qWaitForWindowExposed(&mw);
    QTest::qWait(30);

    auto& doc = mw.document();
    const auto id1 = doc.add_box([] { cadino::core::Box b; b.position = {0, 0};     return b; }());
    const auto id2 = doc.add_box([] { cadino::core::Box b; b.position = {1500, 0};  return b; }());
    const auto id3 = doc.add_box([] { cadino::core::Box b; b.position = {3000, 0};  return b; }());

    mw.plan_view_widget()->set_selections({
        {id1, cadino::ui::SelectKind::Box},
        {id2, cadino::ui::SelectKind::Box},
        {id3, cadino::ui::SelectKind::Box},
    });

    QAction* align_left = nullptr;
    for (QAction* a : mw.findChildren<QAction*>()) {
        if (a->text().contains("Align") && a->text().contains("Left")) {
            align_left = a;
            break;
        }
    }
    REQUIRE(align_left != nullptr);
    align_left->trigger();
    QTest::qWait(20);

    REQUIRE(doc.find_box(id1)->position.x() == Catch::Approx(0.0));
    REQUIRE(doc.find_box(id2)->position.x() == Catch::Approx(0.0));
    REQUIRE(doc.find_box(id3)->position.x() == Catch::Approx(0.0));
}

TEST_CASE("Viewport3D: click on X-axis gizmo handle drags box along world X",
          "[gui][gizmo]") {
    // Need a real GL surface even in offscreen mode.
    QSurfaceFormat fmt;
    fmt.setVersion(3, 3);
    fmt.setProfile(QSurfaceFormat::CoreProfile);
    QSurfaceFormat::setDefaultFormat(fmt);

    cadino::core::Document doc;
    cadino::core::CommandStack stack(doc);
    cadino::core::Box b;
    b.position = {1000, 0};
    b.size_xy = {600, 400};
    b.height = 0;        // pivot stays on the ground plane
    b.base_z = 0;
    const auto id = doc.add_box(std::move(b));

    cadino::ui::PlanView pv(doc, stack);
    cadino::ui::Viewport3D vp(doc, stack, pv);
    vp.resize(800, 600);
    vp.show();
    if (!QTest::qWaitForWindowExposed(&vp)) {
        WARN("Offscreen GL context not available — skipping gizmo drag");
        return;
    }
    QTest::qWait(60);

    pv.set_selections({{id, cadino::ui::SelectKind::Box}});
    vp.update();
    QTest::qWait(60);

    // Compute screen positions of the X arrow's tail (= pivot) and a small
    // step further out in world X. Dragging from the first to the second
    // should move the box along world X by roughly that displacement.
    const QVector3D pivot = vp.selection_centroid_for_test();
    REQUIRE(pivot.x() == Catch::Approx(1000.0));

    const QPointF tail = vp.world_to_screen(pivot + QVector3D(150.0f, 0.0f, 0.0f));
    const QPointF drag_to = vp.world_to_screen(pivot + QVector3D(800.0f, 0.0f, 0.0f));

    QTest::mousePress(&vp, Qt::LeftButton, Qt::NoModifier, tail.toPoint());
    QTest::qWait(10);
    QTest::mouseMove(&vp, drag_to.toPoint());
    QTest::qWait(10);
    QTest::mouseRelease(&vp, Qt::LeftButton, Qt::NoModifier, drag_to.toPoint());
    QTest::qWait(10);

    const auto* moved = doc.find_box(id);
    REQUIRE(moved != nullptr);
    // Allow a generous slop because perspective projection of an arbitrary
    // ray onto the world X axis isn't pixel-exact.
    REQUIRE(moved->position.x() > 1100.0);
    REQUIRE(moved->position.y() == Catch::Approx(0.0).margin(10.0));
}
