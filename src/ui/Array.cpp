#include "Array.hpp"

#include <cmath>
#include <variant>

#include "command/BlockCommands.hpp"
#include "command/BlockInstanceCommands.hpp"
#include "command/BoxCommands.hpp"
#include "command/CommandStack.hpp"
#include "command/CylinderCommands.hpp"
#include "command/DimensionCommands.hpp"
#include "command/NurbsCurveCommands.hpp"
#include "command/NurbsSurfaceCommands.hpp"
#include "command/WallCommands.hpp"
#include "document/Document.hpp"

namespace cadino::ui {

namespace {

using Snapshot = std::variant<
    cadino::core::Wall,
    cadino::core::Box,
    cadino::core::Cylinder,
    cadino::core::NurbsCurve,
    cadino::core::NurbsSurface,
    cadino::core::Block,
    cadino::core::BlockInstance,
    cadino::core::Dimension>;

// Capture the entity's data so we can re-emit transformed copies later
// without re-querying the document each iteration.
std::vector<Snapshot> capture(const cadino::core::Document& doc,
                              const std::vector<Selection>& sels) {
    std::vector<Snapshot> out;
    out.reserve(sels.size());
    for (const auto& sel : sels) {
        switch (sel.kind) {
            case SelectKind::Wall:
                if (const auto* w = doc.find_wall(sel.id)) out.emplace_back(*w);
                break;
            case SelectKind::Box:
                if (const auto* b = doc.find_box(sel.id)) out.emplace_back(*b);
                break;
            case SelectKind::Cylinder:
                if (const auto* c = doc.find_cylinder(sel.id)) out.emplace_back(*c);
                break;
            case SelectKind::NurbsCurve:
                if (const auto* c = doc.find_curve(sel.id)) out.emplace_back(*c);
                break;
            case SelectKind::NurbsSurface:
                if (const auto* s = doc.find_surface(sel.id)) out.emplace_back(*s);
                break;
            case SelectKind::Block:
                if (const auto* b = doc.find_block(sel.id)) out.emplace_back(*b);
                break;
            case SelectKind::BlockInstance:
                if (const auto* i = doc.find_block_instance(sel.id)) out.emplace_back(*i);
                break;
            case SelectKind::Dimension:
                if (const auto* d = doc.find_dimension(sel.id)) out.emplace_back(*d);
                break;
            default: break;
        }
    }
    return out;
}

// Apply an XY transformation (translate + rotate) to one snapshot and
// issue the matching Add command. `angle` is added to any rotation_z
// field. Returns true if a copy was added.
bool emit_transformed(cadino::core::CommandStack& stack,
                      const Snapshot& snap,
                      const Eigen::Vector2d& pivot,
                      double angle,
                      const Eigen::Vector2d& translate) {
    const double c = std::cos(angle);
    const double s = std::sin(angle);
    auto xform = [&](const Eigen::Vector2d& p) {
        const Eigen::Vector2d v = p - pivot;
        const Eigen::Vector2d r{c * v.x() - s * v.y(), s * v.x() + c * v.y()};
        return pivot + r + translate;
    };
    auto xform3 = [&](const Eigen::Vector3d& p) {
        const Eigen::Vector2d r = xform({p.x(), p.y()});
        return Eigen::Vector3d{r.x(), r.y(), p.z()};
    };

    return std::visit([&](const auto& src) -> bool {
        using T = std::decay_t<decltype(src)>;
        T copy = src;
        copy.id = {};
        copy.group_id = {};

        if constexpr (std::is_same_v<T, cadino::core::Wall>) {
            copy.start = xform(src.start);
            copy.end = xform(src.end);
            stack.execute(std::make_unique<cadino::core::AddWallCommand>(std::move(copy)));
            return true;
        } else if constexpr (std::is_same_v<T, cadino::core::Box>) {
            copy.position = xform(src.position);
            copy.rotation_z = src.rotation_z + angle;
            stack.execute(std::make_unique<cadino::core::AddBoxCommand>(std::move(copy)));
            return true;
        } else if constexpr (std::is_same_v<T, cadino::core::Cylinder>) {
            copy.position = xform(src.position);
            stack.execute(std::make_unique<cadino::core::AddCylinderCommand>(std::move(copy)));
            return true;
        } else if constexpr (std::is_same_v<T, cadino::core::NurbsCurve>) {
            for (auto& cp : copy.control_points) cp = xform3(cp);
            stack.execute(std::make_unique<cadino::core::AddNurbsCurveCommand>(std::move(copy)));
            return true;
        } else if constexpr (std::is_same_v<T, cadino::core::NurbsSurface>) {
            for (auto& cp : copy.control_points) cp = xform3(cp);
            stack.execute(std::make_unique<cadino::core::AddNurbsSurfaceCommand>(std::move(copy)));
            return true;
        } else if constexpr (std::is_same_v<T, cadino::core::Block>) {
            copy.position = xform(src.position);
            copy.rotation_z = src.rotation_z + angle;
            stack.execute(std::make_unique<cadino::core::AddBlockCommand>(std::move(copy)));
            return true;
        } else if constexpr (std::is_same_v<T, cadino::core::BlockInstance>) {
            copy.position = xform(src.position);
            copy.rotation_z = src.rotation_z + angle;
            stack.execute(std::make_unique<cadino::core::AddBlockInstanceCommand>(std::move(copy)));
            return true;
        } else if constexpr (std::is_same_v<T, cadino::core::Dimension>) {
            copy.start = xform(src.start);
            copy.end = xform(src.end);
            stack.execute(std::make_unique<cadino::core::AddDimensionCommand>(std::move(copy)));
            return true;
        }
        return false;
    }, snap);
}

}  // namespace

int rectangular_array(cadino::core::Document& doc,
                      cadino::core::CommandStack& stack,
                      const std::vector<Selection>& selections,
                      int rows, int cols, double dx, double dy) {
    if (rows < 1 || cols < 1) return 0;
    const auto snaps = capture(doc, selections);
    if (snaps.empty()) return 0;

    int copies = 0;
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            if (r == 0 && c == 0) continue;
            const Eigen::Vector2d translate{c * dx, r * dy};
            for (const auto& snap : snaps) {
                if (emit_transformed(stack, snap, {0, 0}, 0.0, translate)) ++copies;
            }
        }
    }
    return copies;
}

int polar_array(cadino::core::Document& doc,
                cadino::core::CommandStack& stack,
                const std::vector<Selection>& selections,
                Eigen::Vector2d center,
                int count, double sweep_rad) {
    if (count < 2) return 0;
    const auto snaps = capture(doc, selections);
    if (snaps.empty()) return 0;

    // For a full sweep (≈ 2π) use count steps so the last copy doesn't
    // overlap the original; otherwise use (count-1) steps so the final
    // copy sits at the sweep boundary.
    const bool full = std::abs(std::fmod(sweep_rad, 2.0 * 3.14159265358979323846)) < 1e-6;
    const double step = full
        ? sweep_rad / static_cast<double>(count)
        : sweep_rad / static_cast<double>(count - 1);

    int copies = 0;
    for (int i = 1; i < count; ++i) {
        const double angle = step * static_cast<double>(i);
        for (const auto& snap : snaps) {
            if (emit_transformed(stack, snap, center, angle, {0, 0})) ++copies;
        }
    }
    return copies;
}

}  // namespace cadino::ui
