#pragma once

#include <string>
#include <vector>

#include "Box.hpp"
#include "Cylinder.hpp"
#include "EntityId.hpp"

namespace cadino::core {

// A named template that a BlockInstance refers to. Children are stored in the
// definition's local frame and rendered through each instance's transform.
struct BlockDefinition {
    EntityId id{};
    std::string name{"Definition"};
    std::vector<Box> boxes;
    std::vector<Cylinder> cylinders;
};

}  // namespace cadino::core
