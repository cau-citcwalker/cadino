#include "Clipboard.hpp"

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

Clipboard& Clipboard::instance() {
    static Clipboard inst;
    return inst;
}

void Clipboard::clear() { entries_.clear(); }

void Clipboard::put(const cadino::core::Document& doc,
                    const std::vector<Selection>& selections) {
    entries_.clear();
    for (const auto& sel : selections) {
        switch (sel.kind) {
            case SelectKind::Wall:
                if (const auto* w = doc.find_wall(sel.id)) entries_.emplace_back(*w);
                break;
            case SelectKind::Box:
                if (const auto* b = doc.find_box(sel.id)) entries_.emplace_back(*b);
                break;
            case SelectKind::Cylinder:
                if (const auto* c = doc.find_cylinder(sel.id)) entries_.emplace_back(*c);
                break;
            case SelectKind::NurbsCurve:
                if (const auto* c = doc.find_curve(sel.id)) entries_.emplace_back(*c);
                break;
            case SelectKind::NurbsSurface:
                if (const auto* s = doc.find_surface(sel.id)) entries_.emplace_back(*s);
                break;
            case SelectKind::Block:
                if (const auto* b = doc.find_block(sel.id)) entries_.emplace_back(*b);
                break;
            case SelectKind::BlockInstance:
                if (const auto* i = doc.find_block_instance(sel.id))
                    entries_.emplace_back(*i);
                break;
            case SelectKind::Dimension:
                if (const auto* d = doc.find_dimension(sel.id)) entries_.emplace_back(*d);
                break;
            default: break;
        }
    }
}

std::vector<Selection> Clipboard::paste(cadino::core::Document& doc,
                                        cadino::core::CommandStack& stack,
                                        Eigen::Vector2d offset) const {
    std::vector<Selection> created;
    created.reserve(entries_.size());

    auto shifted_xy = [&](const Eigen::Vector2d& p) { return p + offset; };

    for (const auto& entry : entries_) {
        std::visit([&](const auto& src) {
            using T = std::decay_t<decltype(src)>;
            T copy = src;
            copy.id = {};         // force a fresh id on apply()
            copy.group_id = {};   // detach from any group on the original

            if constexpr (std::is_same_v<T, cadino::core::Wall>) {
                copy.start = shifted_xy(src.start);
                copy.end = shifted_xy(src.end);
                auto cmd = std::make_unique<cadino::core::AddWallCommand>(std::move(copy));
                auto* p = cmd.get();
                stack.execute(std::move(cmd));
                created.push_back({p->entity_id(), SelectKind::Wall});
            } else if constexpr (std::is_same_v<T, cadino::core::Box>) {
                copy.position = shifted_xy(src.position);
                auto cmd = std::make_unique<cadino::core::AddBoxCommand>(std::move(copy));
                auto* p = cmd.get();
                stack.execute(std::move(cmd));
                created.push_back({p->entity_id(), SelectKind::Box});
            } else if constexpr (std::is_same_v<T, cadino::core::Cylinder>) {
                copy.position = shifted_xy(src.position);
                auto cmd = std::make_unique<cadino::core::AddCylinderCommand>(std::move(copy));
                auto* p = cmd.get();
                stack.execute(std::move(cmd));
                created.push_back({p->entity_id(), SelectKind::Cylinder});
            } else if constexpr (std::is_same_v<T, cadino::core::NurbsCurve>) {
                for (auto& cp : copy.control_points) {
                    cp.x() += offset.x();
                    cp.y() += offset.y();
                }
                auto cmd = std::make_unique<cadino::core::AddNurbsCurveCommand>(std::move(copy));
                auto* p = cmd.get();
                stack.execute(std::move(cmd));
                created.push_back({p->entity_id(), SelectKind::NurbsCurve});
            } else if constexpr (std::is_same_v<T, cadino::core::NurbsSurface>) {
                for (auto& cp : copy.control_points) {
                    cp.x() += offset.x();
                    cp.y() += offset.y();
                }
                auto cmd = std::make_unique<cadino::core::AddNurbsSurfaceCommand>(std::move(copy));
                auto* p = cmd.get();
                stack.execute(std::move(cmd));
                created.push_back({p->entity_id(), SelectKind::NurbsSurface});
            } else if constexpr (std::is_same_v<T, cadino::core::Block>) {
                copy.position = shifted_xy(src.position);
                auto cmd = std::make_unique<cadino::core::AddBlockCommand>(std::move(copy));
                auto* p = cmd.get();
                stack.execute(std::move(cmd));
                created.push_back({p->entity_id(), SelectKind::Block});
            } else if constexpr (std::is_same_v<T, cadino::core::BlockInstance>) {
                copy.position = shifted_xy(src.position);
                auto cmd = std::make_unique<cadino::core::AddBlockInstanceCommand>(std::move(copy));
                auto* p = cmd.get();
                stack.execute(std::move(cmd));
                created.push_back({p->entity_id(), SelectKind::BlockInstance});
            } else if constexpr (std::is_same_v<T, cadino::core::Dimension>) {
                copy.start = shifted_xy(src.start);
                copy.end = shifted_xy(src.end);
                auto cmd = std::make_unique<cadino::core::AddDimensionCommand>(std::move(copy));
                auto* p = cmd.get();
                stack.execute(std::move(cmd));
                created.push_back({p->entity_id(), SelectKind::Dimension});
            }
            (void)doc;
        }, entry);
    }
    return created;
}

}  // namespace cadino::ui
