#include "Alignment.hpp"

#include <algorithm>
#include <limits>
#include <optional>

#include <Eigen/Core>

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

// Returns the entity's center in XY world coords. For walls, that's the
// segment midpoint; for everything else it's the entity's `position` field.
std::optional<Eigen::Vector2d> entity_center(const cadino::core::Document& doc,
                                             const Selection& sel) {
    switch (sel.kind) {
        case SelectKind::Wall:
            if (const auto* w = doc.find_wall(sel.id))
                return (w->start + w->end) * 0.5;
            break;
        case SelectKind::Box:
            if (const auto* b = doc.find_box(sel.id)) return b->position;
            break;
        case SelectKind::Cylinder:
            if (const auto* c = doc.find_cylinder(sel.id)) return c->position;
            break;
        case SelectKind::Block:
            if (const auto* b = doc.find_block(sel.id)) return b->position;
            break;
        case SelectKind::BlockInstance:
            if (const auto* i = doc.find_block_instance(sel.id)) return i->position;
            break;
        default: break;
    }
    return std::nullopt;
}

bool set_entity_center(cadino::core::Document& doc,
                       cadino::core::CommandStack& stack,
                       const Selection& sel,
                       const Eigen::Vector2d& target) {
    switch (sel.kind) {
        case SelectKind::Wall: {
            auto* w = doc.find_wall(sel.id);
            if (!w) return false;
            const Eigen::Vector2d cur = (w->start + w->end) * 0.5;
            const Eigen::Vector2d delta = target - cur;
            if (delta.squaredNorm() < 1e-12) return false;
            cadino::core::Wall after = *w;
            after.start += delta;
            after.end += delta;
            stack.execute(std::make_unique<cadino::core::ModifyWallCommand>(sel.id, std::move(after)));
            return true;
        }
        case SelectKind::Box: {
            auto* b = doc.find_box(sel.id);
            if (!b) return false;
            if ((b->position - target).squaredNorm() < 1e-12) return false;
            cadino::core::Box after = *b;
            after.position = target;
            stack.execute(std::make_unique<cadino::core::ModifyBoxCommand>(sel.id, std::move(after)));
            return true;
        }
        case SelectKind::Cylinder: {
            auto* c = doc.find_cylinder(sel.id);
            if (!c) return false;
            if ((c->position - target).squaredNorm() < 1e-12) return false;
            cadino::core::Cylinder after = *c;
            after.position = target;
            stack.execute(std::make_unique<cadino::core::ModifyCylinderCommand>(sel.id, std::move(after)));
            return true;
        }
        case SelectKind::Block: {
            auto* b = doc.find_block(sel.id);
            if (!b) return false;
            if ((b->position - target).squaredNorm() < 1e-12) return false;
            cadino::core::Block after = *b;
            after.position = target;
            stack.execute(std::make_unique<cadino::core::ModifyBlockCommand>(sel.id, std::move(after)));
            return true;
        }
        case SelectKind::BlockInstance: {
            auto* i = doc.find_block_instance(sel.id);
            if (!i) return false;
            if ((i->position - target).squaredNorm() < 1e-12) return false;
            cadino::core::BlockInstance after = *i;
            after.position = target;
            stack.execute(std::make_unique<cadino::core::ModifyBlockInstanceCommand>(sel.id, std::move(after)));
            return true;
        }
        default: break;
    }
    return false;
}

}  // namespace

int apply_alignment(cadino::core::Document& doc,
                    cadino::core::CommandStack& stack,
                    const std::vector<Selection>& selections,
                    AlignMode mode) {
    struct Item {
        Selection sel;
        Eigen::Vector2d center;
    };
    std::vector<Item> items;
    items.reserve(selections.size());
    for (const auto& sel : selections) {
        if (auto c = entity_center(doc, sel)) {
            items.push_back({sel, *c});
        }
    }
    if (items.size() < 2) return 0;

    int modified = 0;
    if (mode == AlignMode::DistributeH || mode == AlignMode::DistributeV) {
        const bool horizontal = (mode == AlignMode::DistributeH);
        std::sort(items.begin(), items.end(),
                  [horizontal](const Item& a, const Item& b) {
                      return horizontal ? a.center.x() < b.center.x()
                                        : a.center.y() < b.center.y();
                  });
        const double lo = horizontal ? items.front().center.x() : items.front().center.y();
        const double hi = horizontal ? items.back().center.x()  : items.back().center.y();
        if (std::abs(hi - lo) < 1e-9) return 0;
        const double step = (hi - lo) / static_cast<double>(items.size() - 1);
        for (std::size_t i = 1; i + 1 < items.size(); ++i) {
            Eigen::Vector2d target = items[i].center;
            if (horizontal) target.x() = lo + step * static_cast<double>(i);
            else            target.y() = lo + step * static_cast<double>(i);
            if (set_entity_center(doc, stack, items[i].sel, target)) ++modified;
        }
        return modified;
    }

    double tx = 0.0, ty = 0.0;
    switch (mode) {
        case AlignMode::Left:
            tx = std::numeric_limits<double>::infinity();
            for (const auto& it : items) tx = std::min(tx, it.center.x());
            break;
        case AlignMode::Right:
            tx = -std::numeric_limits<double>::infinity();
            for (const auto& it : items) tx = std::max(tx, it.center.x());
            break;
        case AlignMode::Top:
            ty = -std::numeric_limits<double>::infinity();
            for (const auto& it : items) ty = std::max(ty, it.center.y());
            break;
        case AlignMode::Bottom:
            ty = std::numeric_limits<double>::infinity();
            for (const auto& it : items) ty = std::min(ty, it.center.y());
            break;
        case AlignMode::CenterH: {
            double sum = 0.0;
            for (const auto& it : items) sum += it.center.x();
            tx = sum / static_cast<double>(items.size());
            break;
        }
        case AlignMode::CenterV: {
            double sum = 0.0;
            for (const auto& it : items) sum += it.center.y();
            ty = sum / static_cast<double>(items.size());
            break;
        }
        default: return 0;
    }

    for (const auto& it : items) {
        Eigen::Vector2d target = it.center;
        switch (mode) {
            case AlignMode::Left:
            case AlignMode::Right:
            case AlignMode::CenterH:
                target.x() = tx; break;
            case AlignMode::Top:
            case AlignMode::Bottom:
            case AlignMode::CenterV:
                target.y() = ty; break;
            default: break;
        }
        if (set_entity_center(doc, stack, it.sel, target)) ++modified;
    }
    return modified;
}

}  // namespace cadino::ui
