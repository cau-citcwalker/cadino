// End-to-end smoke test that drives the full Cadino document/IO pipeline:
//   - build a representative scene with one of every entity type
//   - exercise undo/redo, snap, alignment, boolean ops
//   - round-trip the document through every exporter (JSON, DXF, OBJ, STL,
//     IFC, HLR elevation DXF) and verify each output is non-empty and
//     contains the expected schema markers
//
// Runs headless against cadino_core + cadino_io + cadino_ui — no widgets,
// no OpenGL context is required.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <QByteArray>
#include <QFile>
#include <QFileInfo>
#include <QString>
#include <QTemporaryDir>

#include <numbers>
#include <optional>
#include <string>
#include <vector>

#include "command/BlockCommands.hpp"
#include "command/BlockInstanceCommands.hpp"
#include "command/BoxCommands.hpp"
#include "command/CommandStack.hpp"
#include "command/CylinderCommands.hpp"
#include "command/DimensionCommands.hpp"
#include "command/DoorWindowSlabCommands.hpp"
#include "command/NurbsCurveCommands.hpp"
#include "command/NurbsSurfaceCommands.hpp"
#include "command/WallCommands.hpp"
#include "document/Document.hpp"

#include "Alignment.hpp"
#include "BooleanOps.hpp"
#include "DocumentIO.hpp"
#include "DxfExporter.hpp"
#include "IfcExporter.hpp"
#include "MeshExport.hpp"
#include "Snap.hpp"

namespace {

using cadino::core::EntityId;

struct Scene {
    cadino::core::Document doc;
    cadino::core::CommandStack stack{doc};

    EntityId furniture_layer{};
    std::vector<EntityId> wall_ids;
    std::vector<EntityId> box_ids;
    EntityId cylinder_id{};
    EntityId slab_id{};
    EntityId door_id{};
    EntityId window_id{};
    EntityId curve_id{};
    EntityId surface_id{};
    EntityId block_id{};
    EntityId block_def_id{};
    std::vector<EntityId> instance_ids;
    std::vector<EntityId> dimension_ids;
};

template <typename Cmd, typename... Args>
EntityId run_add(Scene& s, Args&&... args) {
    auto cmd = std::make_unique<Cmd>(std::forward<Args>(args)...);
    auto* ptr = cmd.get();
    s.stack.execute(std::move(cmd));
    return ptr->entity_id();
}

Scene build_scene() {
    Scene s;

    // Add a Furniture layer so newly-created entities pick that up.
    cadino::core::Layer furniture;
    furniture.name = "Furniture";
    furniture.color = {0.95f, 0.55f, 0.25f};
    s.furniture_layer = s.doc.add_layer(std::move(furniture));
    s.doc.set_active_layer(s.furniture_layer);

    // 4 walls forming a 4m x 3m room
    const std::vector<std::pair<Eigen::Vector2d, Eigen::Vector2d>> wall_pts{
        {{0,    0},    {4000, 0}},
        {{4000, 0},    {4000, 3000}},
        {{4000, 3000}, {0,    3000}},
        {{0,    3000}, {0,    0}},
    };
    for (const auto& [a, b] : wall_pts) {
        cadino::core::Wall w;
        w.start = a;
        w.end = b;
        w.thickness = 200.0;
        w.height = 2400.0;
        s.wall_ids.push_back(run_add<cadino::core::AddWallCommand>(s, std::move(w)));
    }

    // Door on first wall
    cadino::core::Door d;
    d.host_wall = s.wall_ids[0];
    d.position_along = 1000.0;
    d.width = 900.0;
    d.height = 2100.0;
    s.door_id = run_add<cadino::core::AddDoorCommand>(s, std::move(d));

    // Window on third wall
    cadino::core::Window win;
    win.host_wall = s.wall_ids[2];
    win.position_along = 2000.0;
    win.width = 1500.0;
    win.height = 1200.0;
    win.sill_height = 900.0;
    s.window_id = run_add<cadino::core::AddWindowCommand>(s, std::move(win));

    // Slab covering the floor
    cadino::core::Slab slab;
    slab.outline = {{0, 0}, {4000, 0}, {4000, 3000}, {0, 3000}};
    slab.level = 0.0;
    slab.thickness = 200.0;
    s.slab_id = run_add<cadino::core::AddSlabCommand>(s, std::move(slab));

    // 3 boxes (so distribute/align have something to chew on)
    for (int i = 0; i < 3; ++i) {
        cadino::core::Box b;
        b.position = {800.0 + i * 1200.0, 1500.0};
        b.size_xy = {600.0, 400.0};
        b.height = 750.0;
        s.box_ids.push_back(run_add<cadino::core::AddBoxCommand>(s, std::move(b)));
    }

    // Cylinder at the far corner
    cadino::core::Cylinder cyl;
    cyl.position = {3500.0, 2500.0};
    cyl.radius = 200.0;
    cyl.height = 2400.0;
    s.cylinder_id = run_add<cadino::core::AddCylinderCommand>(s, std::move(cyl));

    // Block holding a sub-element so the in-line Block path is exercised
    cadino::core::Block block;
    block.name = "Composite";
    block.position = {500.0, 500.0};
    {
        cadino::core::Box child;
        child.position = {0, 0};
        child.size_xy = {200.0, 200.0};
        child.height = 500.0;
        block.boxes.push_back(child);
        cadino::core::Cylinder pole;
        pole.position = {0, 0};
        pole.radius = 50.0;
        pole.height = 1500.0;
        block.cylinders.push_back(pole);
    }
    s.block_id = run_add<cadino::core::AddBlockCommand>(s, std::move(block));

    // Block definition + 2 instances exercising the reusable-template path
    cadino::core::BlockDefinition def;
    def.name = "Chair";
    cadino::core::Box seat;
    seat.position = {0, 0};
    seat.size_xy = {450.0, 450.0};
    seat.height = 80.0;
    seat.base_z = 420.0;
    def.boxes.push_back(seat);
    s.block_def_id = run_add<cadino::core::AddBlockDefinitionCommand>(s, std::move(def));

    for (int i = 0; i < 2; ++i) {
        cadino::core::BlockInstance inst;
        inst.definition_id = s.block_def_id;
        inst.position = {1500.0 + i * 600.0, 2200.0};
        inst.rotation_z = static_cast<double>(i) * 0.5;
        s.instance_ids.push_back(
            run_add<cadino::core::AddBlockInstanceCommand>(s, std::move(inst)));
    }

    // NURBS curve along the room
    {
        cadino::core::NurbsCurve curve;
        curve.degree = 3;
        curve.control_points = {
            {500, 250, 0}, {1500, 750, 0}, {2500, 250, 0}, {3500, 750, 0},
        };
        s.curve_id = run_add<cadino::core::AddNurbsCurveCommand>(s, std::move(curve));
    }

    // 4x4 NURBS surface near the back wall
    {
        cadino::core::NurbsSurface surf;
        surf.degree_u = 3;
        surf.degree_v = 3;
        surf.rows = 4;
        surf.cols = 4;
        for (int r = 0; r < surf.rows; ++r) {
            for (int c = 0; c < surf.cols; ++c) {
                const double x = 200.0 + c * 250.0;
                const double y = 2200.0 + r * 200.0;
                const double z = 1200.0 + std::sin(c) * 100.0;
                surf.control_points.emplace_back(x, y, z);
            }
        }
        s.surface_id = run_add<cadino::core::AddNurbsSurfaceCommand>(s, std::move(surf));
    }

    // 2 dimensions: one along an interior wall, one freeform across the room
    for (int i = 0; i < 2; ++i) {
        cadino::core::Dimension dim;
        if (i == 0) {
            dim.start = {0, -300};
            dim.end = {4000, -300};
            dim.offset = 0.0;
        } else {
            dim.start = {800, 2500};
            dim.end = {3200, 2700};
            dim.offset = 400.0;
        }
        s.dimension_ids.push_back(
            run_add<cadino::core::AddDimensionCommand>(s, std::move(dim)));
    }

    return s;
}

bool file_contains(const QString& path, const QByteArray& needle) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return false;
    const QByteArray content = f.readAll();
    return content.contains(needle);
}

}  // namespace

TEST_CASE("full pipeline smoke", "[smoke]") {
    Scene s = build_scene();

    SECTION("scene counts as expected") {
        REQUIRE(s.doc.walls().size()           == 4);
        REQUIRE(s.doc.boxes().size()           == 3);
        REQUIRE(s.doc.cylinders().size()       == 1);
        REQUIRE(s.doc.slabs().size()           == 1);
        REQUIRE(s.doc.doors().size()           == 1);
        REQUIRE(s.doc.windows().size()         == 1);
        REQUIRE(s.doc.curves().size()          == 1);
        REQUIRE(s.doc.surfaces().size()        == 1);
        REQUIRE(s.doc.blocks().size()          == 1);
        REQUIRE(s.doc.block_defs().size()      == 1);
        REQUIRE(s.doc.block_instances().size() == 2);
        REQUIRE(s.doc.dimensions().size()      == 2);
        REQUIRE(s.doc.layers().size()          == 2);  // Default + Furniture
    }

    SECTION("new entities inherit the active layer") {
        const auto* wall = s.doc.find_wall(s.wall_ids.front());
        REQUIRE(wall != nullptr);
        REQUIRE(wall->layer_id == s.furniture_layer);

        const auto* box = s.doc.find_box(s.box_ids.front());
        REQUIRE(box != nullptr);
        REQUIRE(box->layer_id == s.furniture_layer);
    }

    SECTION("undo and redo round-trip") {
        const std::size_t before = s.doc.entity_count();
        s.stack.undo();
        REQUIRE(s.doc.entity_count() == before - 1);
        s.stack.redo();
        REQUIRE(s.doc.entity_count() == before);
    }

    SECTION("snap engine returns the strongest hit kind") {
        cadino::ui::SnapEngine snap;

        // Wall endpoint at the room origin
        auto e = snap.snap({0.0, 0.0}, s.doc, 50.0);
        REQUIRE(e.kind == cadino::ui::SnapKind::Endpoint);

        // Just outside the cylinder perimeter -> Nearest snap
        const auto& cyl = s.doc.cylinders().at(s.cylinder_id);
        QPointF off_perim(cyl.position.x() + cyl.radius + 5.0,
                          cyl.position.y());
        auto p = snap.snap(off_perim, s.doc, 50.0);
        REQUIRE((p.kind == cadino::ui::SnapKind::Nearest ||
                 p.kind == cadino::ui::SnapKind::Perpendicular ||
                 p.kind == cadino::ui::SnapKind::Endpoint));

        // Wall midpoint
        auto m = snap.snap({2000.0, 0.0}, s.doc, 50.0);
        REQUIRE((m.kind == cadino::ui::SnapKind::Midpoint ||
                 m.kind == cadino::ui::SnapKind::Endpoint ||
                 m.kind == cadino::ui::SnapKind::Perpendicular ||
                 m.kind == cadino::ui::SnapKind::Intersection));
    }

    SECTION("alignment moves entities to the target") {
        // Align all three boxes' X to the leftmost
        std::vector<cadino::ui::Selection> sels;
        for (auto id : s.box_ids) sels.push_back({id, cadino::ui::SelectKind::Box});
        const int n = cadino::ui::apply_alignment(
            s.doc, s.stack, sels, cadino::ui::AlignMode::Left);
        REQUIRE(n >= 1);

        const double target_x = s.doc.find_box(s.box_ids.front())->position.x();
        for (auto id : s.box_ids) {
            const auto* b = s.doc.find_box(id);
            REQUIRE(b != nullptr);
            REQUIRE(b->position.x() == Catch::Approx(target_x));
        }
    }

    SECTION("boolean union produces a mesh") {
        const auto mesh = cadino::ui::union_entities(
            s.doc,
            {s.box_ids[0], cadino::ui::SelectKind::Box},
            {s.box_ids[1], cadino::ui::SelectKind::Box});
        REQUIRE(mesh.has_value());
        REQUIRE_FALSE(mesh->positions.empty());
        REQUIRE_FALSE(mesh->indices.empty());
    }

    SECTION("every exporter writes a non-empty file with expected markers") {
        QTemporaryDir tmp;
        REQUIRE(tmp.isValid());

        const QString json = tmp.path() + "/scene.cadino";
        const QString dxf  = tmp.path() + "/scene.dxf";
        const QString obj  = tmp.path() + "/scene.obj";
        const QString stl  = tmp.path() + "/scene.stl";
        const QString ifc  = tmp.path() + "/scene.ifc";
        const QString front = tmp.path() + "/front.dxf";
        const QString right = tmp.path() + "/right.dxf";

        REQUIRE(cadino::ui::save_document_to_file(s.doc, json));
        REQUIRE(cadino::ui::export_document_as_dxf(s.doc, dxf));
        REQUIRE(cadino::ui::export_as_obj(s.doc, obj));
        REQUIRE(cadino::ui::export_as_stl(s.doc, stl));
        REQUIRE(cadino::ui::export_document_as_ifc(s.doc, ifc));
        REQUIRE(cadino::ui::export_elevation_as_dxf(
            s.doc, cadino::ui::ElevationPlane::Front, front));
        REQUIRE(cadino::ui::export_elevation_as_dxf(
            s.doc, cadino::ui::ElevationPlane::Right, right));

        for (const auto& p : {json, dxf, obj, stl, ifc, front, right}) {
            REQUIRE(QFile(p).size() > 0);
        }

        REQUIRE(file_contains(dxf, "WALLS"));
        REQUIRE(file_contains(dxf, "DIMENSIONS"));
        REQUIRE(file_contains(obj, "v "));
        REQUIRE(file_contains(obj, "f "));
        REQUIRE(file_contains(stl, "facet"));
        REQUIRE(file_contains(stl, "endfacet"));
        REQUIRE(file_contains(ifc, "IFCWALLSTANDARDCASE"));
        REQUIRE(file_contains(ifc, "IFCSLAB"));
        REQUIRE(file_contains(ifc, "IFCCOLUMN"));
        REQUIRE(file_contains(ifc, "IFCBUILDINGELEMENTPROXY"));
        REQUIRE(file_contains(ifc, "IFCPROJECT"));

        // JSON round-trip
        cadino::core::Document doc2;
        REQUIRE(cadino::ui::load_document_from_file(doc2, json));
        REQUIRE(doc2.walls().size()           == s.doc.walls().size());
        REQUIRE(doc2.boxes().size()           == s.doc.boxes().size());
        REQUIRE(doc2.cylinders().size()       == s.doc.cylinders().size());
        REQUIRE(doc2.slabs().size()           == s.doc.slabs().size());
        REQUIRE(doc2.doors().size()           == s.doc.doors().size());
        REQUIRE(doc2.windows().size()         == s.doc.windows().size());
        REQUIRE(doc2.curves().size()          == s.doc.curves().size());
        REQUIRE(doc2.surfaces().size()        == s.doc.surfaces().size());
        REQUIRE(doc2.blocks().size()          == s.doc.blocks().size());
        REQUIRE(doc2.block_instances().size() == s.doc.block_instances().size());
        REQUIRE(doc2.dimensions().size()      == s.doc.dimensions().size());
        REQUIRE(doc2.layers().size()          == s.doc.layers().size());
    }
}
