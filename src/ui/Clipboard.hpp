#pragma once

#include <variant>
#include <vector>

#include <Eigen/Core>

#include "Selection.hpp"
#include "entity/Block.hpp"
#include "entity/BlockInstance.hpp"
#include "entity/Box.hpp"
#include "entity/Cylinder.hpp"
#include "entity/Dimension.hpp"
#include "entity/NurbsCurve.hpp"
#include "entity/NurbsSurface.hpp"
#include "entity/TextAnnotation.hpp"
#include "entity/Wall.hpp"

namespace cadino::core {
class Document;
class CommandStack;
}  // namespace cadino::core

namespace cadino::ui {

// Per-process in-memory clipboard for the geometry editor. Stores
// fully-detached snapshots of selected entities so they can be re-emitted
// with fresh IDs by paste().
class Clipboard {
public:
    using Entry = std::variant<
        cadino::core::Wall,
        cadino::core::Box,
        cadino::core::Cylinder,
        cadino::core::NurbsCurve,
        cadino::core::NurbsSurface,
        cadino::core::Block,
        cadino::core::BlockInstance,
        cadino::core::Dimension,
        cadino::core::TextAnnotation>;

    static Clipboard& instance();

    void clear();
    [[nodiscard]] bool empty() const noexcept { return entries_.empty(); }

    // Capture the current selection's entity data. Entries that don't have
    // a matching factory (slabs, doors, windows) are silently skipped.
    void put(const cadino::core::Document& doc,
             const std::vector<Selection>& selections);

    // Re-emit all clipboard entries through the command stack, translated
    // by `offset` in XY. New ids are assigned by the document; the returned
    // selection list points at the freshly-created entities.
    std::vector<Selection> paste(cadino::core::Document& doc,
                                 cadino::core::CommandStack& stack,
                                 Eigen::Vector2d offset) const;

private:
    Clipboard() = default;
    std::vector<Entry> entries_;
};

}  // namespace cadino::ui
