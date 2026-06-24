#pragma once

#include <string>

#include "Color.hpp"
#include "EntityId.hpp"

namespace cadino::core {

struct Layer {
    EntityId id{};
    std::string name{"Layer"};
    Color color{0.85f, 0.85f, 0.85f};
    bool visible{true};
    bool locked{false};
};

}  // namespace cadino::core
