#pragma once

#include <vector>

#include <Eigen/Core>

#include "Selection.hpp"

namespace cadino::core {
class Document;
class CommandStack;
}  // namespace cadino::core

namespace cadino::ui {

// Duplicate `selections` into a rows x cols grid offset by (dx, dy) mm
// from the originals. The (0,0) cell stays as the source; every other
// cell gets a fresh copy. Returns the number of copies created.
int rectangular_array(cadino::core::Document& doc,
                      cadino::core::CommandStack& stack,
                      const std::vector<Selection>& selections,
                      int rows, int cols, double dx, double dy);

// Duplicate `selections` around `center` with `count` total copies
// distributed across `sweep_rad` radians (set to 2π for a full circle).
// The first instance is the original; (count - 1) rotated copies are
// added. Returns the number of copies created.
int polar_array(cadino::core::Document& doc,
                cadino::core::CommandStack& stack,
                const std::vector<Selection>& selections,
                Eigen::Vector2d center,
                int count, double sweep_rad);

}  // namespace cadino::ui
