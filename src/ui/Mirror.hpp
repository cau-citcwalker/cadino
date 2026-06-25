#pragma once

#include <vector>

#include <Eigen/Core>

#include "Selection.hpp"

namespace cadino::core {
class Document;
class CommandStack;
}  // namespace cadino::core

namespace cadino::ui {

// Reflect the given selections about the line through `axis_p` with the
// 2D unit direction `axis_dir`. When `copy` is true the originals stay
// and reflected duplicates are added; otherwise the originals are
// modified in place. Returns the number of entities affected (or
// created in copy mode).
int mirror_selection(cadino::core::Document& doc,
                     cadino::core::CommandStack& stack,
                     const std::vector<Selection>& selections,
                     Eigen::Vector2d axis_p,
                     Eigen::Vector2d axis_dir,
                     bool copy);

}  // namespace cadino::ui
