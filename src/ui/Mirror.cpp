#include "Mirror.hpp"

#include <cmath>

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

Eigen::Vector2d reflect2(const Eigen::Vector2d& q,
                         const Eigen::Vector2d& p,
                         const Eigen::Vector2d& d) {
    const Eigen::Vector2d v = q - p;
    const double par = v.dot(d);
    const Eigen::Vector2d v_par = par * d;
    const Eigen::Vector2d v_perp = v - v_par;
    return p + v_par - v_perp;
}

// Issue an Add command, returning the new entity's id via the command's
// captured copy.
template <typename Cmd, typename Entity>
cadino::core::EntityId emit_add(cadino::core::CommandStack& stack, Entity e) {
    auto cmd = std::make_unique<Cmd>(std::move(e));
    auto* p = cmd.get();
    stack.execute(std::move(cmd));
    return p->entity_id();
}

}  // namespace

int mirror_selection(cadino::core::Document& doc,
                     cadino::core::CommandStack& stack,
                     const std::vector<Selection>& selections,
                     Eigen::Vector2d axis_p,
                     Eigen::Vector2d axis_dir,
                     bool copy) {
    const double len = axis_dir.norm();
    if (len < 1e-9) return 0;
    const Eigen::Vector2d d = axis_dir / len;
    const double phi = std::atan2(d.y(), d.x());
    const double two_phi = 2.0 * phi;

    auto mirror_pt3 = [&](Eigen::Vector3d p) {
        const Eigen::Vector2d r = reflect2({p.x(), p.y()}, axis_p, d);
        return Eigen::Vector3d{r.x(), r.y(), p.z()};
    };

    int affected = 0;
    for (const auto& sel : selections) {
        switch (sel.kind) {
            case SelectKind::Wall: {
                const auto* w = doc.find_wall(sel.id);
                if (!w) break;
                cadino::core::Wall after = *w;
                after.start = reflect2(w->start, axis_p, d);
                after.end   = reflect2(w->end,   axis_p, d);
                if (copy) {
                    after.id = {};
                    after.group_id = {};
                    emit_add<cadino::core::AddWallCommand>(stack, std::move(after));
                } else {
                    stack.execute(std::make_unique<cadino::core::ModifyWallCommand>(
                        sel.id, std::move(after)));
                }
                ++affected;
                break;
            }
            case SelectKind::Box: {
                const auto* b = doc.find_box(sel.id);
                if (!b) break;
                cadino::core::Box after = *b;
                after.position = reflect2(b->position, axis_p, d);
                after.rotation_z = two_phi - b->rotation_z;
                if (copy) {
                    after.id = {};
                    after.group_id = {};
                    emit_add<cadino::core::AddBoxCommand>(stack, std::move(after));
                } else {
                    stack.execute(std::make_unique<cadino::core::ModifyBoxCommand>(
                        sel.id, std::move(after)));
                }
                ++affected;
                break;
            }
            case SelectKind::Cylinder: {
                const auto* c = doc.find_cylinder(sel.id);
                if (!c) break;
                cadino::core::Cylinder after = *c;
                after.position = reflect2(c->position, axis_p, d);
                if (copy) {
                    after.id = {};
                    after.group_id = {};
                    emit_add<cadino::core::AddCylinderCommand>(stack, std::move(after));
                } else {
                    stack.execute(std::make_unique<cadino::core::ModifyCylinderCommand>(
                        sel.id, std::move(after)));
                }
                ++affected;
                break;
            }
            case SelectKind::Block: {
                const auto* b = doc.find_block(sel.id);
                if (!b) break;
                cadino::core::Block after = *b;
                after.position = reflect2(b->position, axis_p, d);
                after.rotation_z = two_phi - b->rotation_z;
                if (copy) {
                    after.id = {};
                    after.group_id = {};
                    emit_add<cadino::core::AddBlockCommand>(stack, std::move(after));
                } else {
                    stack.execute(std::make_unique<cadino::core::ModifyBlockCommand>(
                        sel.id, std::move(after)));
                }
                ++affected;
                break;
            }
            case SelectKind::BlockInstance: {
                const auto* i = doc.find_block_instance(sel.id);
                if (!i) break;
                cadino::core::BlockInstance after = *i;
                after.position = reflect2(i->position, axis_p, d);
                after.rotation_z = two_phi - i->rotation_z;
                if (copy) {
                    after.id = {};
                    after.group_id = {};
                    emit_add<cadino::core::AddBlockInstanceCommand>(stack, std::move(after));
                } else {
                    stack.execute(std::make_unique<cadino::core::ModifyBlockInstanceCommand>(
                        sel.id, std::move(after)));
                }
                ++affected;
                break;
            }
            case SelectKind::NurbsCurve: {
                const auto* c = doc.find_curve(sel.id);
                if (!c) break;
                cadino::core::NurbsCurve after = *c;
                for (auto& cp : after.control_points) cp = mirror_pt3(cp);
                if (copy) {
                    after.id = {};
                    after.group_id = {};
                    emit_add<cadino::core::AddNurbsCurveCommand>(stack, std::move(after));
                } else {
                    stack.execute(std::make_unique<cadino::core::ModifyNurbsCurveCommand>(
                        sel.id, std::move(after)));
                }
                ++affected;
                break;
            }
            case SelectKind::NurbsSurface: {
                const auto* s = doc.find_surface(sel.id);
                if (!s) break;
                cadino::core::NurbsSurface after = *s;
                for (auto& cp : after.control_points) cp = mirror_pt3(cp);
                if (copy) {
                    after.id = {};
                    after.group_id = {};
                    emit_add<cadino::core::AddNurbsSurfaceCommand>(stack, std::move(after));
                } else {
                    stack.execute(std::make_unique<cadino::core::ModifyNurbsSurfaceCommand>(
                        sel.id, std::move(after)));
                }
                ++affected;
                break;
            }
            case SelectKind::Dimension: {
                const auto* dim = doc.find_dimension(sel.id);
                if (!dim) break;
                cadino::core::Dimension after = *dim;
                after.start = reflect2(dim->start, axis_p, d);
                after.end   = reflect2(dim->end,   axis_p, d);
                // Perpendicular direction flips under reflection; the
                // signed offset relative to the new (start, end) needs to
                // flip too so the dimension line stays on the same side
                // of the geometry.
                after.offset = -dim->offset;
                if (copy) {
                    after.id = {};
                    after.group_id = {};
                    emit_add<cadino::core::AddDimensionCommand>(stack, std::move(after));
                } else {
                    stack.execute(std::make_unique<cadino::core::ModifyDimensionCommand>(
                        sel.id, std::move(after)));
                }
                ++affected;
                break;
            }
            default: break;
        }
    }
    return affected;
}

}  // namespace cadino::ui
