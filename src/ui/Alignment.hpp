#pragma once

#include <vector>

#include "Selection.hpp"

namespace cadino::core {
class Document;
class CommandStack;
}  // namespace cadino::core

namespace cadino::ui {

enum class AlignMode {
    Left, Right, Top, Bottom, CenterH, CenterV,
    DistributeH, DistributeV,
};

// Apply an alignment / distribution operation to the given selections.
// Returns the number of entities actually modified.
int apply_alignment(cadino::core::Document& doc,
                    cadino::core::CommandStack& stack,
                    const std::vector<Selection>& selections,
                    AlignMode mode);

}  // namespace cadino::ui
